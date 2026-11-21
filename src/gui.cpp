#include <raven.hpp>
#include "keyboard_preview.hpp"
#include "media_knob.hpp"
#include "lighting_profiles.hpp"
#include "battery_state.hpp"
#include "device_state.hpp"
#include "tool_runner.hpp"
#include "ui_components.hpp"
#include "desktop_integration.hpp"
#include "ui_text.hpp"
#include "preview_cadence.hpp"
#include "last_lighting.hpp"
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <future>
#include <functional>
#include <filesystem>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <cstdio>

int main(int argc, char** argv) {
    bool test = false, backgroundLaunch = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--test") test = true;
        else if (std::string(argv[i]) == "--background") backgroundLaunch = true;
    }
    wchar_t executablePath[32768]{}; GetModuleFileNameW(nullptr, executablePath, 32768);
    wchar_t localData[32768]{};
    const DWORD localLength = GetEnvironmentVariableW(L"LOCALAPPDATA", localData, 32768);
    const auto dataFolder = localLength > 0 && localLength < 32768
        ? std::filesystem::path(localData) / L"AulaControl" : std::filesystem::path{};
    GeneralPreferenceStore generalStore(test || dataFolder.empty() ? std::filesystem::path{} : dataFolder / L"preferences.txt");
    auto preferences = test ? GeneralPreferences{} : generalStore.load();
    SingleInstance instance;
    if (!test) {
        const auto result = instance.acquire(!backgroundLaunch);
        if (result == SingleInstance::Result::existing) return 0;
        if (result == SingleInstance::Result::error) {
            MessageBoxW(nullptr, preferences.language == i18n::Language::english
                ? L"Could not check whether AULA Control is already open."
                : L"No se pudo comprobar si AULA Control ya está abierta.", L"AULA Control", MB_OK | MB_ICONERROR); return 1;
        }
    }
    StartupRegistration startup(executablePath);
    const auto startupEnabled = test ? std::optional<bool>(false) : startup.enabled();
    DesktopTray tray;
    RInitDesc desc{}; desc.backend = test ? R_BACKEND_NULL : R_BACKEND_VULKAN; desc.headless = test; desc.app_name = "AULA Control";
    if (!raven::init_ex(&desc)) return 1;
    UiText texts(preferences.language);
    tray.set_language(preferences.language);
    raven::set_animation_speed(1.25f);
    const auto assetPath = std::filesystem::path(executablePath).parent_path() / L"assets" / L"keyboard-actual-v1.png";
    const keyboard_model::Model keyboardModel(assetPath);
    ui::apply_theme(ui::Skin::neumorphic);
    auto window = raven::Window::create("AULA Control", 1440, 900, R_WINDOW_HIDDEN);
    window.set_min_size(900, 760); window.set_clear_color(0.12f, 0.135f, 0.155f, 1);
    RNativeWindow native{}; window.get_native_handle(&native);
    const auto nativeWindow = reinterpret_cast<HWND>(native.window);
    bool restoreMaximized = false, hiddenToTray = false, exitRequested = false;
    if (!test && preferences.rememberWindow && preferences.bounds) {
        restore_window_bounds(nativeWindow, *preferences.bounds); restoreMaximized = preferences.bounds->maximized;
    }
    auto panel = [](RWidgetType type, float gap) { auto p = raven::Panel(raven::Widget::create(type)); p.set_spacing(gap); return p; };
    std::vector<ui::Surface> surfaces;
    auto card = [&](float padding, ui::SurfaceRole role = ui::SurfaceRole::panel) { auto value = ui::Surface::create(padding, role); surfaces.push_back(value); return value; };
    struct LabelStyle { raven::Label widget; float red, green, blue; };
    std::vector<LabelStyle> labelStyles;
    auto label = [&](const char* text, float size, float r, float g, float b) {
        auto l = texts.label(text); l.set_text_size(size); l.set_color(r,g,b,1);
        labelStyles.push_back({l, r, g, b}); return l;
    };
    auto shell = panel(R_WIDGET_HBOX, 26); shell.slot_set_padding(24, 24, 24, 24);
    auto sidebar = card(16, ui::SurfaceRole::sidebar); sidebar.set_size(204, -1);
    auto sidebarBox = panel(R_WIDGET_VBOX, 9);
    auto logo = label("AULA Control", 20, 0.94f, 0.97f, 0.99f);
    sidebarBox.add_child(logo);
    constexpr int homePageIndex = 4;
    constexpr int preferencesPageIndex = 6;
    const std::array<const char*, 7> pageNames{{"Iluminación", "Perilla", "Perfiles", "Dispositivo", "Inicio", "Apariencia", "Preferencias"}};
    std::array<ui::NavigationItem, 7> navigation{{ui::navigation(pageNames[0], 0), ui::navigation(pageNames[1], 1), ui::navigation(pageNames[2], 2), ui::navigation(pageNames[3], 3), ui::navigation(pageNames[4], 4), ui::navigation(pageNames[5], 5), ui::navigation(pageNames[6], 6)}};
    const std::array<int, 7> navigationOrder{{4, 0, 1, 2, 3, 5, 6}};
    for (const int index : navigationOrder) {
        texts.set(navigation[index].text, pageNames[index]);
        texts.accessible(navigation[index].button, pageNames[index]);
        texts.tooltip(navigation[index].button, pageNames[index]);
        sidebarBox.add_child(navigation[index].button);
    }
    auto sidebarSpace = raven::Widget::create(R_WIDGET_SPACER); sidebarSpace.slot_set_fill(1); sidebarBox.add_child(sidebarSpace);
    auto sidebarDevice = label("AULA F75", 15, 0.89f, 0.93f, 0.96f);
    sidebarBox.add_child(sidebarDevice); sidebar.add_child(sidebarBox);
    auto root = panel(R_WIDGET_VBOX, 20); root.slot_set_fill(1);
    shell.add_child(sidebar); shell.add_child(root);
    auto header = panel(R_WIDGET_HBOX, 20);
    auto brand = panel(R_WIDGET_VBOX, 5);
    auto title = label(pageNames[0], 32, 0.94f, 0.96f, 0.98f);
    brand.add_child(title); brand.slot_set_fill(1);
    BatteryState batteryState(BatteryState::Clock::now());
    batteryState.set_interval(preferences.batteryInterval, BatteryState::Clock::now());
    DeviceState deviceState(DeviceState::Clock::now());
    float connectionRed = .62f, connectionGreen = .72f, connectionBlue = .80f;
    auto connectionRow = panel(R_WIDGET_HBOX, 8);
    auto connectionDot = raven::Custom::create(); connectionDot.set_size(8, 18); connectionDot.set_accessible_hidden(R_TRUE);
    connectionDot.set_paint_callback([&](Rid, RPainter* raw, float, float) { raven::Painter(raw).circle(4, 9, 3.5f, connectionRed, connectionGreen, connectionBlue, 1); });
    auto connectionLabel = label("Comprobando receptor...", 12, .65f, .74f, .81f); connectionLabel.set_wrap(R_TRUE); connectionLabel.slot_set_fill(1);
    connectionRow.add_child(connectionDot); connectionRow.add_child(connectionLabel); brand.add_child(connectionRow);
    auto batteryCard = card(16, ui::SurfaceRole::compact); batteryCard.set_size(240, -1);
    auto batteryBox = panel(R_WIDGET_VBOX, 6);
    auto batteryTitle = label("BATERÍA  /  2.4 GHz", 12, 0.54f, 0.78f, 0.84f);
    auto batteryValue = label("--", 27, 0.93f, 0.96f, 0.98f);
    auto batteryBar = raven::ProgressBar::create(); batteryBar.set_size(-1, 6);
    auto batteryDetail = label("Esperando primera lectura", 12, 0.62f, 0.68f, 0.75f);
    batteryBox.add_child(batteryTitle); batteryBox.add_child(batteryValue); batteryBox.add_child(batteryBar); batteryBox.add_child(batteryDetail); batteryCard.add_child(batteryBox);
    header.add_child(brand); header.add_child(batteryCard); root.add_child(header);
    auto startupPage = panel(R_WIDGET_VBOX, 0); startupPage.slot_set_padding(28, 28, 28, 28);
    auto startupTop = raven::Widget::create(R_WIDGET_SPACER); startupTop.slot_set_fill(1); startupPage.add_child(startupTop);
    auto startupRow = panel(R_WIDGET_HBOX, 0);
    auto startupLeft = raven::Widget::create(R_WIDGET_SPACER); startupLeft.slot_set_fill(1); startupRow.add_child(startupLeft);
    auto startupCard = card(32, ui::SurfaceRole::hero); startupCard.set_size(560, -1);
    auto startupBox = panel(R_WIDGET_VBOX, 16);
    auto startupTitle = label("Iniciando", 32, .94f, .96f, .98f); startupTitle.set_wrap(R_TRUE); startupBox.add_child(startupTitle);
    auto startupSpinner = raven::Custom::create(); startupSpinner.set_size(-1, 90); startupSpinner.set_accessible_hidden(R_TRUE);
    const auto startupBegan = std::chrono::steady_clock::now();
    startupSpinner.set_paint_callback([&](Rid, RPainter* raw, float w, float h) {
        raven::Painter p(raw);
        const float t = raven::get_animations_enabled() ? static_cast<float>(std::chrono::duration<double>(std::chrono::steady_clock::now() - startupBegan).count()) : 0.0f;
        for (int i = 0; i < 36; ++i) {
            const float a = i * 6.2831853f / 36.0f, b = (i + .72f) * 6.2831853f / 36.0f;
            const float alpha = .12f + .7f * (.5f + .5f * std::sin(a - t * 3.0f));
            p.line(w * .5f + 32 * std::cos(a), h * .5f + 32 * std::sin(a), w * .5f + 32 * std::cos(b), h * .5f + 32 * std::sin(b), 3, .20f, .80f, .91f, alpha);
        }
        p.rect_outline(w * .5f - 15, h * .5f - 9, 30, 18, 4, 1.5f, .78f, .90f, .96f, .85f);
        for (int row = 0; row < 2; ++row) for (int col = 0; col < 5; ++col)
            p.rect(w * .5f - 10 + col * 4.5f, h * .5f - 4 + row * 5, 2, 2, .6f, .78f, .90f, .96f, .85f);
    });
    startupBox.add_child(startupSpinner);
    auto startupMessage = label("Buscando receptor...", 17, .86f, .92f, .96f); startupMessage.set_wrap(R_TRUE); startupBox.add_child(startupMessage);
    auto startupProfiles = label("", 13, .62f, .70f, .78f); startupProfiles.set_wrap(R_TRUE); startupBox.add_child(startupProfiles);
    auto enterApp = texts.button("Continuar"); enterApp.set_style(R_BUTTON_PRIMARY); startupBox.add_child(enterApp);
    startupCard.add_child(startupBox); startupRow.add_child(startupCard);
    auto startupRight = raven::Widget::create(R_WIDGET_SPACER); startupRight.slot_set_fill(1); startupRow.add_child(startupRight);
    startupPage.add_child(startupRow);
    auto startupBottom = raven::Widget::create(R_WIDGET_SPACER); startupBottom.slot_set_fill(1); startupPage.add_child(startupBottom);
    auto stage = raven::Border::create(); stage.set_padding(14, 14, 14, 14); stage.slot_set_fill(1);
    stage.set_background(0.11f, 0.127f, 0.15f, 1); stage.set_corner_radius(18, 18, 18, 18);
    stage.set_outline(1, 0.35f, 0.40f, 0.48f, 0.20f);
    auto stageBox = panel(R_WIDGET_VBOX, 4);
    auto stageTitle = label("Vista previa", 11, 0.65f, 0.75f, 0.82f);
    auto keyboard = raven::Custom::create(); keyboard.set_min_size(-1, 210); keyboard.slot_set_fill(1);
    float previewR = 0.56f, previewG = 0.2f, previewB = 0.04f, previewPower = 1.0f; int previewMode = 1, previewSpeed = 3; bool previewMulticolor = false;
    LastLightingStore lastLightingStore(test || dataFolder.empty() ? std::filesystem::path{} : dataFolder / L"last-lighting.txt");
    const auto rememberedLighting = test ? std::optional<LastLighting>{} : lastLightingStore.load();
    std::optional<LightingSettings> lastSent = rememberedLighting ? std::optional<LightingSettings>(rememberedLighting->settings) : std::optional<LightingSettings>{};
    std::optional<LightingSettings> sendingSettings;
    ControlTransport sendingTransport = ControlTransport::receiver;
    unsigned sendingGeneration = 0;
    bool automaticLightingJob = false, draftMatchesSent = false;
    std::optional<unsigned> appliedGeneration;
    LightingRestorePolicy lightingRestore;
    LightingTimeline draftTimeline, appliedTimeline;
    if (rememberedLighting) appliedTimeline.restart(rememberedLighting->settings.speed,
        lighting_elapsed_seconds(rememberedLighting->receipt.timeMs, lighting_wall_time_ms()), LightingTimeline::Clock::now());
    auto previewTime = [&]() {
        return (draftMatchesSent ? appliedTimeline : draftTimeline).sample(LightingTimeline::Clock::now(), raven::get_animations_enabled() != 0);
    };
    keyboard.set_paint_callback([&](Rid, RPainter* painter, float w, float h) {
        const double time = previewTime();
        draw_keyboard(painter, w, h, previewR, previewG, previewB, previewPower, previewMode, time, previewMulticolor, keyboardModel);
    });
    texts.tooltip(stageTitle, "Vista previa de la selección. Inicio muestra el último efecto enviado. La fase de animación es aproximada.");
    stageBox.add_child(stageTitle); stageBox.add_child(keyboard); stage.add_child(stageBox);
    auto content = panel(R_WIDGET_HBOX, 24); content.slot_set_fill(1);
    auto colorCard = card(22, ui::SurfaceRole::hero); colorCard.slot_set_fill(1);
    auto colorBox = panel(R_WIDGET_VBOX, 10);
    auto pickerTitle = label("Color", 21, 0.94f, 0.96f, 0.98f);
    auto editState = label("Cambios pendientes", 12, 0.95f, 0.72f, 0.35f);
    editState.set_wrap(R_TRUE);
    auto picker = raven::color_picker_create(); raven::color_picker_set_alpha_enabled(picker, R_FALSE); raven::color_picker_set_color(picker, 0.56f, 0.2f, 0.04f, 1);
    raven::color_picker_add_swatch(picker, 1.0f, 0.20f, 0.27f, 1);
    raven::color_picker_add_swatch(picker, 1.0f, 0.64f, 0.15f, 1);
    raven::color_picker_add_swatch(picker, 0.15f, 0.85f, 0.52f, 1);
    raven::color_picker_add_swatch(picker, 0.15f, 0.76f, 0.90f, 1);
    raven::color_picker_add_swatch(picker, 0.58f, 0.37f, 1.0f, 1);
    raven::color_picker_add_swatch(picker, 1.0f, 1.0f, 1.0f, 1);
    raven::color_picker_set_popup(picker, R_TRUE);
    raven::Widget(picker).set_size(58, 38); texts.tooltip(raven::Widget(picker), "Abrir selector de color");
    auto paletteRow = panel(R_WIDGET_HBOX, 9); paletteRow.add_child(raven::Widget(picker));
    struct QuickColor { const char* name; float red, green, blue; };
    const std::array<QuickColor, 6> quickColors{{{"Coral", 1, .20f, .27f}, {"Ámbar", 1, .64f, .15f}, {"Menta", .15f, .85f, .52f}, {"Cian", .15f, .76f, .90f}, {"Violeta", .58f, .37f, 1}, {"Blanco", 1, 1, 1}}};
    std::array<raven::Button, 6> swatches;
    for (std::size_t i = 0; i < swatches.size(); ++i) {
        const auto& c = quickColors[i]; auto button = texts.button(""); button.set_size(30, 30); button.slot_set_valign(R_VALIGN_CENTER);
        button.set_metric(R_METRIC_CORNER_RADIUS, 10); texts.accessible(button, c.name); texts.tooltip(button, c.name);
        button.set_color(R_COLOR_CONTROL, c.red, c.green, c.blue, 1);
        button.set_color(R_COLOR_CONTROL_HOVERED, c.red, c.green, c.blue, .8f);
        button.set_color(R_COLOR_CONTROL_PRESSED, c.red * .7f, c.green * .7f, c.blue * .7f, 1);
        button.set_color(R_COLOR_CONTROL_BORDER, 1, 1, 1, .16f); swatches[i] = button; paletteRow.add_child(button);
    }
    auto draftBadge = raven::Border::create(); draftBadge.set_padding(10, 6, 10, 6);
    draftBadge.set_corner_radius(10, 10, 10, 10); draftBadge.set_background(.27f, .20f, .10f, .26f);
    draftBadge.add_child(editState);
    colorBox.add_child(stage); colorBox.add_child(pickerTitle); colorBox.add_child(paletteRow); colorBox.add_child(draftBadge); colorCard.add_child(colorBox);
    auto settingsCard = card(24); settingsCard.set_size(360, -1);
    auto settings = panel(R_WIDGET_VBOX, 6);
    auto settingsTitle = label("Configuración", 21, 0.94f, 0.96f, 0.98f);
    std::function<void()> updatePreview;
    std::function<void()> refreshDraft;
    std::function<void()> refreshHome;
    std::function<void()> refreshBackground;
    auto selectionCard = raven::Border::create();
    selectionCard.set_background(0.105f, 0.12f, 0.15f, 1);
    selectionCard.set_corner_radius(16, 16, 16, 16);
    selectionCard.set_outline(1, 0.25f, 0.28f, 0.32f, 0.8f);
    selectionCard.set_padding(14, 8, 14, 8);
    auto selectionRow = panel(R_WIDGET_HBOX, 12);
    auto colorChip = raven::Border::create(); colorChip.set_size(38, 38);
    colorChip.set_corner_radius(12, 12, 12, 12);
    colorChip.set_outline(1, 0.85f, 0.85f, 0.90f, 0.3f);
    colorChip.set_background(0.56f, 0.2f, 0.04f, 1);
    auto paletteIndicator = raven::Custom::create(); paletteIndicator.set_size(38, 38); paletteIndicator.set_accessible_hidden(R_TRUE);
    paletteIndicator.set_paint_callback([&](Rid, RPainter* raw, float, float) {
        if (!previewMulticolor && !lighting_uses_builtin_palette(previewMode)) return;
        raven::Painter p(raw);
        p.circle(14, 15, 8, 1, .28f, .36f, .95f);
        p.circle(24, 15, 8, .20f, .86f, .55f, .95f);
        p.circle(19, 24, 8, .24f, .65f, 1, .95f);
    });
    colorChip.add_child(paletteIndicator);
    auto selectionText = panel(R_WIDGET_VBOX, 3);
    auto selectedTitle = label("COLOR SELECCIONADO", 11, 0.58f, 0.68f, 0.76f);
    auto selectedHex = label(raven::color_picker_get_hex(picker), 16, 0.94f, 0.96f, 0.98f);
    selectionText.add_child(selectedTitle); selectionText.add_child(selectedHex);
    selectionRow.add_child(colorChip); selectionRow.add_child(selectionText); selectionCard.add_child(selectionRow);
    raven::color_picker_set_changed_callback(picker, [&](Rid, float, float, float, float) {
        if (updatePreview) updatePreview();
    });
    auto effectLabel = label("EFECTO", 12, 0.65f, 0.74f, 0.81f);
    const auto& effects = kLightingEffects;
    auto effect = raven::ComboBox::create(); for (const auto& e : effects) texts.add_item(effect, e.name); effect.set_selected(0);
    effect.set_changed_callback([&](Rid, int index) {
        if (index >= 0 && static_cast<std::size_t>(index) < effects.size()) {
            previewMode = effects[static_cast<std::size_t>(index)].mode;
            if (updatePreview) updatePreview();
        }
    });
    auto paletteLabel = label("PALETA", 12, 0.65f, 0.74f, 0.81f);
    auto palette = raven::ComboBox::create(); texts.add_item(palette, "Color único"); texts.add_item(palette, "RGB arcoíris"); palette.set_selected(0);
    auto nativeBrightnessLabel = label("BRILLO DEL TECLADO   3 / 5", 12, .65f, .74f, .81f);
    auto nativeBrightness = raven::Slider::create(); nativeBrightness.set_range(1, 5); nativeBrightness.set_step(1); nativeBrightness.set_value(3); nativeBrightness.set_size(-1, 30);
    texts.accessible(nativeBrightness, "Brillo del teclado");
    texts.tooltip(nativeBrightness, "Nivel de 1 a 5. Pulsa Aplicar para enviarlo.");
    nativeBrightness.set_changed_callback([&](Rid, float) { if (updatePreview) updatePreview(); });
    auto brightnessLabel = label("AJUSTE RGB   100%", 11, .65f, .74f, .81f); brightnessLabel.set_size(125, 30); brightnessLabel.slot_set_valign(R_VALIGN_CENTER);
    auto brightness = raven::Slider::create(); brightness.set_range(0, 100); brightness.set_step(1); brightness.set_value(100); brightness.set_size(-1, 30);
    texts.accessible(brightness, "Ajuste fino RGB del color"); texts.tooltip(brightness, "Atenúa el color único de 0 a 100 %.");
    brightness.slot_set_fill(1);
    brightness.set_changed_callback([&](Rid, float) { if (updatePreview) updatePreview(); });
    auto fineRgb = panel(R_WIDGET_HBOX, 10); fineRgb.add_child(brightnessLabel); fineRgb.add_child(brightness);
    auto speedLabel = label("VELOCIDAD   3 / 5", 12, .65f, .74f, .81f);
    auto speed = raven::Slider::create(); speed.set_range(1, 5); speed.set_step(1); speed.set_value(3); speed.set_size(-1, 30);
    texts.accessible(speed, "Velocidad del efecto");
    texts.tooltip(speed, "Nivel de 1 a 5. Pulsa Aplicar para enviarlo.");
    speed.set_changed_callback([&](Rid, float) { if (updatePreview) updatePreview(); });
    auto sliderValues = [&]() { return std::array<float, 3>{{brightness.get_value(), speed.get_value(), nativeBrightness.get_value()}}; };
    auto renderedSliders = sliderValues();
    auto note = label("", 12, 0.59f, 0.65f, 0.72f);
    note.set_wrap(R_TRUE);
    auto syncPalette = [&](int index) {
        previewMulticolor = index == 1;
        if (updatePreview) updatePreview();
    };
    palette.set_changed_callback([&](Rid, int index) { syncPalette(index); });
    auto apply = texts.button("Aplicar al teclado"); apply.set_style(R_BUTTON_PRIMARY); apply.set_size(-1, 44); apply.set_enabled(R_FALSE);
    auto recoverLighting = texts.button("Recuperar último envío"); recoverLighting.set_style(R_BUTTON_FLAT); recoverLighting.set_size(-1, 32); recoverLighting.set_enabled(R_FALSE);
    texts.tooltip(recoverLighting, "Recupera los ajustes del último envío guardado.");
    auto status = label(!test && !lastLightingStore.readable() ? "No se pudo leer el último efecto guardado." : "", 13, 0.71f, 0.78f, 0.83f);
    status.set_wrap(R_TRUE);
    texts.tooltip(apply, "Envía los ajustes seleccionados.");
    settings.add_child(settingsTitle); settings.add_child(selectionCard); settings.add_child(effectLabel); settings.add_child(effect); settings.add_child(paletteLabel); settings.add_child(palette);
    settings.add_child(nativeBrightnessLabel); settings.add_child(nativeBrightness); settings.add_child(fineRgb); settings.add_child(note);
    settings.add_child(speedLabel); settings.add_child(speed);
    auto actionSpace = raven::Widget::create(R_WIDGET_SPACER); actionSpace.slot_set_fill(1); settings.add_child(actionSpace);
    settings.add_child(apply); settings.add_child(recoverLighting); settings.add_child(status);
    updatePreview = [&]() {
        float alpha; raven::color_picker_get_color(picker, &previewR, &previewG, &previewB, &alpha);
        const int selectedSpeed = static_cast<int>(std::lround(std::clamp(speed.get_value(), 1.0f, 5.0f)));
        if (selectedSpeed != previewSpeed) {
            draftTimeline.set_speed(selectedSpeed, LightingTimeline::Clock::now(), raven::get_animations_enabled() != 0);
            previewSpeed = selectedSpeed;
        }
        const int rgbLevel = static_cast<int>(std::lround(std::clamp(brightness.get_value(), 0.0f, 100.0f)));
        const auto rgbText = "AJUSTE RGB   " + std::to_string(rgbLevel) + "%";
        texts.set(brightnessLabel, rgbText.c_str());
        const bool builtIn = lighting_uses_builtin_palette(previewMode), colorControl = !previewMulticolor && !builtIn;
        raven::Widget(picker).set_enabled(colorControl);
        for (const auto& swatch : swatches) swatch.set_enabled(colorControl);
        brightness.set_enabled(colorControl);
        fineRgb.set_visibility(colorControl ? R_VISIBLE : R_COLLAPSED); fineRgb.set_accessible_hidden(!colorControl);
        speed.set_enabled(lighting_has_speed(previewMode));
        const auto speedText = lighting_has_speed(previewMode) ? "VELOCIDAD   " + std::to_string(previewSpeed) + " / 5" : std::string("VELOCIDAD   ·   EFECTO ESTÁTICO");
        texts.set(speedLabel, speedText.c_str());
        const int nativeLevel = static_cast<int>(std::lround(std::clamp(nativeBrightness.get_value(), 1.0f, 5.0f)));
        const auto nativeText = "BRILLO DEL TECLADO   " + std::to_string(nativeLevel) + " / 5";
        texts.set(nativeBrightnessLabel, nativeText.c_str());
        // A visual approximation, independent of the firmware's actual light curve.
        previewPower = (colorControl ? std::clamp(brightness.get_value(), 0.0f, 100.0f) / 100.0f : 1.0f) * (nativeLevel / 5.0f);
        colorChip.set_background(colorControl ? previewR : .08f, colorControl ? previewG : .10f, colorControl ? previewB : .15f, 1);
        paletteIndicator.invalidate();
        texts.set(selectedTitle, colorControl ? "COLOR SELECCIONADO" : "PALETA DEL EFECTO");
        texts.set(selectedHex, !colorControl ? "Paleta del teclado" : raven::color_picker_get_hex(picker));
        texts.set(note, builtIn ? "Este efecto usa su propia paleta." : "");
        note.set_visibility(builtIn ? R_VISIBLE : R_COLLAPSED);
        if (refreshDraft) refreshDraft();
        if (refreshBackground) refreshBackground();
        renderedSliders = sliderValues();
        keyboard.invalidate();
    };
    updatePreview();
    // Raven's accessibility value setter changes slider values without invoking
    // Changed. Reconcile those edits on the existing UI loop as well.
    auto synchronizeSliders = [&]() { if (sliderValues() != renderedSliders) updatePreview(); };
    for (std::size_t i = 0; i < swatches.size(); ++i) swatches[i].set_clicked_callback([&, i](Rid) {
        const auto& color = quickColors[i]; raven::color_picker_set_color(picker, color.red, color.green, color.blue, 1); updatePreview();
    });
    settingsCard.add_child(settings); content.add_child(colorCard); content.add_child(settingsCard);

    MediaKnob mediaKnob;
    bool mediaClicksActive = false;
    auto knobCard = card(26);
    auto knobBox = panel(R_WIDGET_VBOX, 14);
    knobBox.add_child(label("Controles multimedia", 26, 0.94f, 0.96f, 0.98f));
    knobBox.add_child(label("MODO MULTIMEDIA", 12, 0.30f, 0.80f, 0.90f));
    knobBox.add_child(label("Mantén pulsada la perilla 3–5 segundos para cambiar entre iluminación y volumen.", 14, 0.71f, 0.78f, 0.83f));
    auto knobActions = panel(R_WIDGET_HBOX, 16);
    const std::array<const char*, 2> actionTitles{{"GIRO IZQUIERDO", "GIRO DERECHO"}};
    const std::array<const char*, 2> actionValues{{"Bajar volumen", "Subir volumen"}};
    for (std::size_t i = 0; i < actionTitles.size(); ++i) {
        auto actionCard = card(18); actionCard.slot_set_fill(1);
        auto actionBox = panel(R_WIDGET_VBOX, 10);
        actionBox.add_child(label(actionTitles[i], 11, 0.54f, 0.78f, 0.84f));
        auto value = label(actionValues[i], 18, 0.94f, 0.96f, 0.98f);
        actionBox.add_child(value); actionCard.add_child(actionBox); knobActions.add_child(actionCard);
    }
    knobBox.add_child(knobActions);
    auto remapCard = card(20);
    auto remapBox = panel(R_WIDGET_VBOX, 12);
    auto remap = texts.checkbox("Activar clics multimedia");
    remap.set_checked(preferences.mediaClicks);
    remapBox.add_child(remap);
    auto clickActions = panel(R_WIDGET_HBOX, 12);
    std::array<raven::Label, 3> clickValues;
    const std::array<const char*, 3> clickTitles{{"UN CLIC", "DOS CLICS", "TRES CLICS"}};
    for (std::size_t i = 0; i < clickValues.size(); ++i) {
        auto clickCard = card(14, ui::SurfaceRole::compact); clickCard.slot_set_fill(1);
        auto clickBox = panel(R_WIDGET_VBOX, 8);
        clickBox.add_child(label(clickTitles[i], 11, .54f, .78f, .84f));
        clickValues[i] = label(i == 0 ? "Silencio" : "—", 17, .94f, .96f, .98f);
        clickValues[i].set_wrap(R_TRUE); clickBox.add_child(clickValues[i]);
        clickCard.add_child(clickBox); clickActions.add_child(clickCard);
    }
    remapBox.add_child(clickActions);
    auto remapScope = label("Reasigna Silencio de todos los dispositivos y programas de Windows.", 14, 0.71f, 0.78f, 0.83f); remapScope.set_wrap(R_TRUE); remapBox.add_child(remapScope);
    auto remapMemory = label("La opción se recuerda al abrir AULA Control.", 13, .62f, .68f, .75f); remapMemory.set_wrap(R_TRUE); remapBox.add_child(remapMemory);
    texts.tooltip(remap, "Filtra duplicados durante 100 ms. Uno o dos clics esperan hasta 400 ms; tres responden al soltar el tercero. Las pulsaciones largas se ignoran.");
    auto knobStatus = label("Desactivado", 14, 0.54f, 0.78f, 0.84f);
    remapBox.add_child(knobStatus); remapCard.add_child(remapBox); knobBox.add_child(remapCard);
    knobCard.add_child(knobBox);
    auto pages = raven::Switcher::create(); pages.slot_set_fill(1);
    pages.add_child(content); pages.add_child(knobCard);
    std::function<void(int)> navigate;

    const auto profilesPath = dataFolder.empty() ? std::filesystem::path{} : dataFolder / L"lighting-profiles.txt";
    const auto appearancePath = profilesPath.empty() ? std::filesystem::path{} : profilesPath.parent_path() / L"appearance.txt";
    auto appearance = test ? ui::Appearance{} : ui::load_appearance(appearancePath);
    ProfileStore profiles(profilesPath);
    if (!test) profiles.load();
    texts.set(startupProfiles, profiles.readable() ? "" : "No se pudieron cargar los perfiles.");
    auto profilesPage = panel(R_WIDGET_HBOX, 20);
    auto saveCard = card(24); saveCard.slot_set_fill(1);
    auto saveBox = panel(R_WIDGET_VBOX, 12);
    saveBox.add_child(label("Guardar perfil", 24, 0.94f, 0.96f, 0.98f));
    saveBox.add_child(label("Selección actual", 12, 0.30f, 0.80f, 0.90f));
    auto draftEffect = label("", 18, 0.94f, 0.96f, 0.98f); draftEffect.set_wrap(R_TRUE);
    auto draftColor = label("", 14, 0.71f, 0.78f, 0.83f); draftColor.set_wrap(R_TRUE);
    saveBox.add_child(draftEffect); saveBox.add_child(draftColor);
    saveBox.add_child(label("NOMBRE DEL PERFIL", 12, 0.65f, 0.74f, 0.81f));
    auto profileName = raven::TextInput::create(); texts.placeholder(profileName, "Nombre del perfil"); profileName.set_max_length(32);
    saveBox.add_child(profileName);
    auto saveProfile = texts.button("Guardar"); saveProfile.set_style(R_BUTTON_PRIMARY);
    saveBox.add_child(saveProfile);
    auto saveHint = label("El mismo nombre actualiza el perfil. Guardar no aplica cambios.", 13, 0.62f, 0.68f, 0.75f); saveHint.set_wrap(R_TRUE); saveBox.add_child(saveHint);
    auto profileStatus = label(profiles.readable() ? "" : "No se pudieron leer los perfiles.", 13, 0.54f, 0.78f, 0.84f);
    profileStatus.set_wrap(R_TRUE); saveBox.add_child(profileStatus); saveCard.add_child(saveBox);

    auto libraryCard = card(24); libraryCard.slot_set_fill(1);
    auto libraryBox = panel(R_WIDGET_VBOX, 12);
    libraryBox.add_child(label("Perfiles guardados", 24, 0.94f, 0.96f, 0.98f));
    auto libraryCount = label("", 13, 0.62f, 0.68f, 0.75f); libraryBox.add_child(libraryCount);
    auto profileList = raven::ComboBox::create(); libraryBox.add_child(profileList);
    auto profileEffect = label("", 18, 0.94f, 0.96f, 0.98f); profileEffect.set_wrap(R_TRUE);
    auto profileColor = label("", 14, 0.71f, 0.78f, 0.83f); profileColor.set_wrap(R_TRUE);
    libraryBox.add_child(profileEffect); libraryBox.add_child(profileColor);
    auto createFirstProfile = texts.button("Crear perfil"); createFirstProfile.set_style(R_BUTTON_PRIMARY);
    libraryBox.add_child(createFirstProfile);
    auto loadProfile = texts.button("Cargar en vista previa"); loadProfile.set_style(R_BUTTON_PRIMARY); libraryBox.add_child(loadProfile);
    auto loadHint = label("Cargar no aplica cambios al teclado.", 13, 0.62f, 0.68f, 0.75f); loadHint.set_wrap(R_TRUE); libraryBox.add_child(loadHint);
    auto libraryActions = panel(R_WIDGET_HBOX, 12);
    auto deleteProfile = texts.button("Eliminar"); deleteProfile.slot_set_fill(1);
    auto undoDelete = texts.button("Deshacer borrado"); undoDelete.slot_set_fill(1); undoDelete.set_enabled(R_FALSE);
    libraryActions.add_child(deleteProfile); libraryActions.add_child(undoDelete); libraryBox.add_child(libraryActions);
    libraryCard.add_child(libraryBox); profilesPage.add_child(saveCard); profilesPage.add_child(libraryCard);
    pages.add_child(profilesPage);

    auto readDraft = [&]() {
        float r, g, b, a; raven::color_picker_get_color(picker, &r, &g, &b, &a);
        const auto channel = [](float v) { return static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };
        return LightingSettings{channel(r), channel(g), channel(b), previewMode,
            static_cast<int>(std::lround(std::clamp(brightness.get_value(), 0.0f, 100.0f))), previewMulticolor,
            static_cast<int>(std::lround(std::clamp(speed.get_value(), 1.0f, 5.0f))),
            static_cast<int>(std::lround(std::clamp(nativeBrightness.get_value(), 1.0f, 5.0f)))};
    };
    auto effectName = [&](const LightingSettings& s) {
        for (const auto& e : effects) if (e.mode == s.mode) return e.name;
        return "Efecto no disponible";
    };
    auto colorDescription = [](const LightingSettings& s) {
        const auto tempo = lighting_has_speed(s.mode) ? " · Velocidad " + std::to_string(s.speed) + "/5" : std::string{};
        const auto native = " · Brillo " + std::to_string(s.brightness) + "/5";
        if (lighting_uses_builtin_palette(s.mode)) return "Paleta propia del efecto" + native + tempo;
        if (s.multicolor) return (lighting_multicolor_verified(s.mode) ? std::string("RGB arcoíris") : std::string("RGB arcoíris · por verificar")) + native + tempo;
        return "RGB " + std::to_string(s.red) + ", " + std::to_string(s.green) + ", " + std::to_string(s.blue)
            + " · Ajuste " + std::to_string(s.intensity) + "%" + native + tempo;
    };
    refreshDraft = [&]() {
        const auto s = readDraft();
        texts.set(draftEffect, effectName(s)); texts.set(draftColor, colorDescription(s).c_str());
        const bool same = lastSent && s == *lastSent;
        draftMatchesSent = same;
        const bool sentHere = appliedGeneration && *appliedGeneration == deviceState.generation();
        texts.set(editState, same ? (sentHere ? "Último envío" : "Último guardado") : "Cambios pendientes");
        editState.set_color(same ? 0.54f : 0.95f, same ? 0.78f : 0.72f, same ? 0.84f : 0.35f, 1);
        draftBadge.set_background(same ? .10f : .27f, same ? .29f : .20f, same ? .32f : .10f, .26f);
        recoverLighting.set_enabled(lastSent.has_value());
        if (refreshHome) refreshHome();
    };
    auto restoreDraft = [&](const LightingSettings& s) {
        if (!s.valid()) return;
        raven::color_picker_set_color(picker, s.red / 255.0f, s.green / 255.0f, s.blue / 255.0f, 1);
        previewMode = s.mode;
        draftTimeline.set_speed(s.speed, LightingTimeline::Clock::now(), raven::get_animations_enabled() != 0);
        previewSpeed = s.speed; speed.set_value(static_cast<float>(s.speed));
        for (std::size_t i = 0; i < effects.size(); ++i) if (effects[i].mode == s.mode) effect.set_selected(static_cast<int>(i));
        brightness.set_value(static_cast<float>(s.intensity));
        nativeBrightness.set_value(static_cast<float>(s.brightness));
        palette.set_selected(s.multicolor ? 1 : 0); syncPalette(s.multicolor ? 1 : 0);
    };
    auto showProfile = [&]() {
        const int index = profileList.get_selected();
        const bool selected = index >= 0 && static_cast<std::size_t>(index) < profiles.entries().size();
        loadProfile.set_enabled(selected); deleteProfile.set_enabled(selected);
        saveProfile.set_enabled(profiles.readable() && valid_profile({profile_name(profileName.get_text()), readDraft()}));
        if (!selected) { texts.set(profileEffect, "No hay perfiles"); texts.set(profileColor, ""); return; }
        const auto& p = profiles.entries()[static_cast<std::size_t>(index)];
        profileName.set_text(p.name.c_str());
        saveProfile.set_enabled(profiles.readable());
        texts.set(profileEffect, effectName(p.settings)); texts.set(profileColor, colorDescription(p.settings).c_str());
    };
    auto refreshLibrary = [&](int index) {
        profileList.clear_items();
        for (const auto& p : profiles.entries()) profileList.add_item(p.name.c_str());
        profileList.set_enabled(!profiles.entries().empty());
        profileList.set_visibility(profiles.entries().empty() ? R_COLLAPSED : R_VISIBLE);
        createFirstProfile.set_visibility(profiles.entries().empty() ? R_VISIBLE : R_COLLAPSED);
        loadProfile.set_visibility(profiles.entries().empty() ? R_COLLAPSED : R_VISIBLE);
        deleteProfile.set_visibility(profiles.entries().empty() ? R_COLLAPSED : R_VISIBLE);
        profileList.set_selected(profiles.entries().empty() ? -1 : std::clamp(index, 0, static_cast<int>(profiles.entries().size()) - 1));
        const auto count = std::to_string(profiles.entries().size()) + " / 24 perfiles";
        texts.set(libraryCount, count.c_str()); showProfile();
        if (refreshHome) refreshHome();
    };
    profileName.set_changed_callback([&](Rid, const char* text) {
        saveProfile.set_enabled(profiles.readable() && valid_profile({profile_name(text), readDraft()}));
    });
    createFirstProfile.set_clicked_callback([&](Rid) {
        texts.set(status, ""); navigate(0);
    });
    auto recoverLastLighting = [&]() {
        if (!lastSent) return;
        restoreDraft(*lastSent); texts.set(status, "Último envío cargado en vista previa.");
    };
    recoverLighting.set_clicked_callback([&](Rid) { recoverLastLighting(); });
    profileList.set_changed_callback([&](Rid, int) { showProfile(); });
    saveProfile.set_clicked_callback([&](Rid) {
        const auto name = profile_name(profileName.get_text());
        const auto result = profiles.save({name, readDraft()});
        if (result == ProfileResult::saved || result == ProfileResult::updated) {
            int index = 0;
            for (std::size_t i = 0; i < profiles.entries().size(); ++i) if (profiles.entries()[i].name == name) index = static_cast<int>(i);
            refreshLibrary(index); texts.set(profileStatus, result == ProfileResult::saved ? "Perfil guardado." : "Perfil actualizado.");
        } else if (result == ProfileResult::invalid) texts.set(profileStatus, "Escribe un nombre válido de hasta 32 caracteres.");
        else if (result == ProfileResult::full) texts.set(profileStatus, "Límite de 24 perfiles. Actualiza o elimina uno.");
        else texts.set(profileStatus, "No se pudo guardar el perfil.");
    });
    loadProfile.set_clicked_callback([&](Rid) {
        const int index = profileList.get_selected();
        if (index < 0 || static_cast<std::size_t>(index) >= profiles.entries().size()) return;
        restoreDraft(profiles.entries()[static_cast<std::size_t>(index)].settings);
        texts.set(status, "Perfil cargado. Pulsa Aplicar."); navigate(0);
        keyboard.play_transition(R_TRANSITION_FADE, 0);
    });
    std::optional<LightingProfile> deletedProfile;
    deleteProfile.set_clicked_callback([&](Rid) {
        const int index = profileList.get_selected();
        if (index < 0 || static_cast<std::size_t>(index) >= profiles.entries().size()) return;
        const auto deleted = profiles.entries()[static_cast<std::size_t>(index)];
        if (!profiles.erase(static_cast<std::size_t>(index))) { texts.set(profileStatus, "No se pudo eliminar el perfil."); return; }
        deletedProfile = deleted; undoDelete.set_enabled(R_TRUE); refreshLibrary(index);
        texts.set(profileStatus, "Perfil eliminado.");
    });
    undoDelete.set_clicked_callback([&](Rid) {
        if (!deletedProfile) return;
        for (const auto& p : profiles.entries()) if (p.name == deletedProfile->name) {
            texts.set(profileStatus, "Ese nombre ya existe."); return;
        }
        if (profiles.save(*deletedProfile) != ProfileResult::saved) { texts.set(profileStatus, "No se pudo restaurar el perfil."); return; }
        deletedProfile.reset(); undoDelete.set_enabled(R_FALSE);
        refreshLibrary(static_cast<int>(profiles.entries().size()) - 1); texts.set(profileStatus, "Perfil restaurado.");
    });
    refreshLibrary(0);
    if (lastSent) restoreDraft(*lastSent);
    else refreshDraft();

    auto devicePage = panel(R_WIDGET_HBOX, 20);
    auto deviceCard = card(26); deviceCard.slot_set_fill(1);
    auto deviceBox = panel(R_WIDGET_VBOX, 14);
    deviceBox.add_child(label("Estado del teclado", 26, 0.94f, 0.96f, 0.98f));
    auto deviceBatteryTitle = label("BATERÍA  /  RECEPTOR 2.4 GHz", 12, 0.30f, 0.80f, 0.90f); deviceBox.add_child(deviceBatteryTitle);
    auto deviceBattery = label("--", 62, 0.93f, 0.96f, 0.98f);
    auto deviceBar = raven::ProgressBar::create(); deviceBar.set_size(-1, 10);
    auto deviceStatus = label("Esperando consulta", 18, 0.54f, 0.78f, 0.84f); deviceStatus.set_wrap(R_TRUE);
    auto lastReading = label("Sin lectura válida", 14, 0.71f, 0.78f, 0.83f); lastReading.set_wrap(R_TRUE);
    deviceBox.add_child(deviceBattery); deviceBox.add_child(deviceBar); deviceBox.add_child(deviceStatus); deviceBox.add_child(lastReading);
    texts.tooltip(deviceBar, "Valor informado por el teclado. Precisión sin calibrar."); deviceCard.add_child(deviceBox);
    auto pollingCard = card(26); pollingCard.slot_set_fill(1);
    auto pollingBox = panel(R_WIDGET_VBOX, 14);
    pollingBox.add_child(label("Actualización automática", 24, 0.94f, 0.96f, 0.98f));
    pollingBox.add_child(label("INTERVALO DE CONSULTA", 12, 0.65f, 0.74f, 0.81f));
    auto pollingInterval = raven::ComboBox::create(); texts.add_item(pollingInterval, "Cada 30 segundos"); texts.add_item(pollingInterval, "Cada 60 segundos"); pollingInterval.set_selected(preferences.batteryInterval == 30 ? 0 : 1);
    pollingBox.add_child(pollingInterval);
    auto pollingPreferenceNote = label("", 12, .62f, .70f, .78f); pollingPreferenceNote.set_wrap(R_TRUE); pollingBox.add_child(pollingPreferenceNote);
    auto nextReading = label("Esperando consulta", 14, 0.54f, 0.78f, 0.84f); nextReading.set_wrap(R_TRUE); pollingBox.add_child(nextReading);
    auto receiverHint = label("Comprobando teclado...", 13, 0.62f, 0.68f, 0.75f); receiverHint.set_wrap(R_TRUE); pollingBox.add_child(receiverHint);
    pollingCard.add_child(pollingBox); devicePage.add_child(deviceCard); devicePage.add_child(pollingCard); pages.add_child(devicePage);
    auto homePage = panel(R_WIDGET_HBOX, 24);
    auto homeHero = card(24, ui::SurfaceRole::hero); homeHero.slot_set_fill(1);
    auto homeHeroBox = panel(R_WIDGET_VBOX, 12);
    homeHeroBox.add_child(label("AULA F75", 26, .94f, .96f, .98f));
    auto homeConnection = label("2.4 GHz", 13, .62f, .68f, .75f); homeHeroBox.add_child(homeConnection);
    auto homeKeyboard = raven::Custom::create(); homeKeyboard.set_min_size(-1, 200); homeKeyboard.slot_set_fill(1);
    homeKeyboard.set_paint_callback([&](Rid, RPainter* painter, float w, float h) {
        if (!lastSent) { draw_keyboard(painter, w, h, 0, 0, 0, 0, 1, 0, false, keyboardModel); return; }
        const auto& s = *lastSent;
        const float fine = s.multicolor || lighting_uses_builtin_palette(s.mode) ? 1.0f : s.intensity / 100.0f;
        const auto time = appliedTimeline.sample(LightingTimeline::Clock::now(), raven::get_animations_enabled() != 0);
        draw_keyboard(painter, w, h, s.red / 255.0f, s.green / 255.0f, s.blue / 255.0f,
            fine * s.brightness / 5.0f, s.mode, time, s.multicolor, keyboardModel);
    });
    homeHeroBox.add_child(homeKeyboard);
    auto homePreviewHint = label("Último enviado", 12, .62f, .68f, .75f); homePreviewHint.set_wrap(R_TRUE); homeHeroBox.add_child(homePreviewHint);
    texts.tooltip(homePreviewHint, "Último efecto enviado por AULA. No lee cambios hechos desde el teclado. La fase de animación es aproximada.");
    auto homeLightingButton = texts.button("Iluminación"); homeLightingButton.set_style(R_BUTTON_PRIMARY); homeHeroBox.add_child(homeLightingButton);
    homeHero.add_child(homeHeroBox);
    auto homeSummary = panel(R_WIDGET_VBOX, 14); homeSummary.set_size(360, -1);
    auto homeSelectionCard = card(18, ui::SurfaceRole::compact); auto homeSelectionBox = panel(R_WIDGET_VBOX, 8);
    homeSelectionBox.add_child(label("ILUMINACIÓN", 11, .30f, .80f, .90f));
    auto homeEffect = label("", 18, .94f, .96f, .98f); homeEffect.set_wrap(R_TRUE);
    auto homeColor = label("", 13, .71f, .78f, .83f); homeColor.set_wrap(R_TRUE);
    auto homeDraftState = label("", 12, .95f, .72f, .35f); homeDraftState.set_wrap(R_TRUE);
    homeSelectionBox.add_child(homeEffect); homeSelectionBox.add_child(homeColor); homeSelectionBox.add_child(homeDraftState); homeSelectionCard.add_child(homeSelectionBox); homeSummary.add_child(homeSelectionCard);
    auto homeProfilesCard = card(18, ui::SurfaceRole::compact); auto homeProfilesBox = panel(R_WIDGET_VBOX, 8);
    homeProfilesBox.add_child(label("PERFILES", 11, .30f, .80f, .90f));
    auto homeProfileCount = label("", 19, .94f, .96f, .98f); homeProfileCount.set_wrap(R_TRUE); homeProfilesBox.add_child(homeProfileCount);
    auto homeProfilesButton = texts.button("Perfiles"); homeProfilesBox.add_child(homeProfilesButton); homeProfilesCard.add_child(homeProfilesBox); homeSummary.add_child(homeProfilesCard);
    auto homeKnobCard = card(18, ui::SurfaceRole::compact); auto homeKnobBox = panel(R_WIDGET_VBOX, 8);
    homeKnobBox.add_child(label("PERILLA", 11, .30f, .80f, .90f));
    auto homeKnobState = label("Clic: Silencio", 18, .94f, .96f, .98f); homeKnobState.set_wrap(R_TRUE); homeKnobBox.add_child(homeKnobState);
    auto homeKnobButton = texts.button("Perilla"); homeKnobBox.add_child(homeKnobButton); homeKnobCard.add_child(homeKnobBox); homeSummary.add_child(homeKnobCard);
    auto homeSpacer = raven::Widget::create(R_WIDGET_SPACER); homeSpacer.slot_set_fill(1); homeSummary.add_child(homeSpacer);
    auto homeDeviceButton = texts.button("Ver batería"); homeSummary.add_child(homeDeviceButton);
    homePage.add_child(homeHero); homePage.add_child(homeSummary); pages.add_child(homePage);
    homeLightingButton.set_clicked_callback([&](Rid) { navigate(0); });
    homeProfilesButton.set_clicked_callback([&](Rid) { navigate(2); });
    homeKnobButton.set_clicked_callback([&](Rid) { navigate(1); });
    homeDeviceButton.set_clicked_callback([&](Rid) { navigate(3); });
    refreshHome = [&]() {
        texts.set(homeEffect, lastSent ? effectName(*lastSent) : "Sin efecto guardado");
        texts.set(homeColor, lastSent ? colorDescription(*lastSent) : "");
        texts.set(homeDraftState, lastSent ? texts.source(editState) : "");
        const auto state = readDraft();
        const bool sent = lastSent && state == *lastSent;
        homeDraftState.set_color(sent ? .54f : .95f, sent ? .78f : .72f, sent ? .84f : .35f, 1);
        const auto size = profiles.entries().size();
        const auto count = !profiles.readable() ? std::string("Biblioteca no disponible")
            : size == 0 ? std::string("Sin perfiles") : std::to_string(size) + (size == 1 ? " perfil guardado" : " perfiles guardados");
        texts.set(homeProfileCount, count.c_str());
        texts.set(homeKnobState, mediaClicksActive ? "Clics multimedia activos" : "Clic: Silencio"); homeKeyboard.invalidate();
    };
    refreshHome();
    auto appearancePage = panel(R_WIDGET_HBOX, 20);
    auto appearanceCard = card(26); appearanceCard.slot_set_fill(1);
    auto appearanceBox = panel(R_WIDGET_VBOX, 14);
    auto appearanceHeading = label("Tema", 26, .94f, .96f, .98f); appearanceHeading.set_wrap(R_TRUE);
    appearanceBox.add_child(appearanceHeading);
    appearanceBox.add_child(label("Tema", 12, .65f, .74f, .81f));
    auto skinChoice = raven::ComboBox::create(); texts.add_item(skinChoice, "Glass"); texts.add_item(skinChoice, "Neumorfismo");
    texts.accessible(skinChoice, "Skin de la aplicación"); skinChoice.set_selected(appearance.skin == ui::Skin::glass ? 0 : 1);
    appearanceBox.add_child(skinChoice);
    auto atmosphereLabel = label("Fondo", 12, .65f, .74f, .81f); appearanceBox.add_child(atmosphereLabel);
    auto atmosphereChoice = raven::ComboBox::create(); texts.add_item(atmosphereChoice, "Aurora"); texts.add_item(atmosphereChoice, "Color seleccionado");
    texts.accessible(atmosphereChoice, "Ambiente del fondo"); atmosphereChoice.set_selected(appearance.atmosphere == ui::Atmosphere::aurora ? 0 : 1);
    appearanceBox.add_child(atmosphereChoice); texts.tooltip(atmosphereChoice, "En multicolor se usa Aurora.");
    auto appearanceSpace = raven::Widget::create(R_WIDGET_SPACER); appearanceSpace.slot_set_fill(1); appearanceBox.add_child(appearanceSpace);
    auto appearanceStatus = label("", 13, .62f, .68f, .75f); appearanceStatus.set_wrap(R_TRUE); appearanceBox.add_child(appearanceStatus);
    appearanceCard.add_child(appearanceBox); appearancePage.add_child(appearanceCard);
    auto materialCard = card(26); materialCard.slot_set_fill(1);
    auto materialBox = panel(R_WIDGET_VBOX, 14);
    auto materialTitle = label("Glass", 30, .94f, .96f, .98f); materialBox.add_child(materialTitle);
    auto materialDemo = raven::Custom::create(); materialDemo.set_min_size(-1, 180); materialDemo.slot_set_fill(1); materialDemo.set_accessible_hidden(R_TRUE);
    materialDemo.set_paint_callback([&](Rid, RPainter* raw, float w, float h) {
        raven::Painter p(raw);
        const float size = (std::min)(w * .72f, h * .72f), x = (w - size) * .5f, y = (h - size) * .5f;
        p.circle(x + size * .24f, y + size * .25f, size * .31f, .12f, .68f, .80f, .45f);
        p.circle(x + size * .80f, y + size * .8f, size * .30f, .56f, .28f, .72f, .45f);
        if (appearance.skin == ui::Skin::glass) {
            p.glass(x, y, size, size * .7f, 22, .12f, .20f, .28f, .20f);
            p.glass(x + size * .22f, y + size * .38f, size * .85f, size * .60f, 22, .18f, .22f, .30f, .26f);
        } else {
            p.rect(x, y, size, size * .7f, 22, .17f, .19f, .22f, 1);
            p.rect(x + size * .22f, y + size * .38f, size * .85f, size * .60f, 22, .13f, .15f, .18f, 1);
        }
        p.rect(x + size * .12f, y + size * .15f, size * .35f, 5, 2.5f, .84f, .94f, 1, .8f);
        p.rect(x + size * .12f, y + size * .24f, size * .20f, 4, 2, .84f, .94f, 1, .35f);
        p.rect(x + size * .36f, y + size * .58f, size * .34f, 5, 2.5f, .84f, .94f, 1, .65f);
        p.rect(x + size * .36f, y + size * .69f, size * .51f, 4, 2, .84f, .94f, 1, .25f);
    });
    materialBox.add_child(materialDemo);
    materialCard.add_child(materialBox); appearancePage.add_child(materialCard); pages.add_child(appearancePage);
    auto preferencesPage = panel(R_WIDGET_HBOX, 20);
    auto behaviorCard = card(26); behaviorCard.slot_set_fill(1);
    auto behaviorBox = panel(R_WIDGET_VBOX, 14);
    auto behaviorHeading = label("Ventana", 26, .94f, .96f, .98f); behaviorHeading.set_wrap(R_TRUE); behaviorBox.add_child(behaviorHeading);
    auto closeToTray = texts.checkbox("Cerrar a la bandeja"); closeToTray.set_checked(preferences.closeToTray); behaviorBox.add_child(closeToTray);
    auto minimizeToTray = texts.checkbox("Minimizar a la bandeja"); minimizeToTray.set_checked(preferences.minimizeToTray); behaviorBox.add_child(minimizeToTray);
    auto rememberWindow = texts.checkbox("Recordar tamaño y posición"); rememberWindow.set_checked(preferences.rememberWindow); behaviorBox.add_child(rememberWindow);
    auto behaviorSpace = raven::Widget::create(R_WIDGET_SPACER); behaviorSpace.slot_set_fill(1); behaviorBox.add_child(behaviorSpace);
    auto hideNow = texts.button("Ocultar en la bandeja"); hideNow.set_style(R_BUTTON_PRIMARY); behaviorBox.add_child(hideNow);
    auto quitApp = texts.button("Salir de AULA Control"); behaviorBox.add_child(quitApp);
    for (const auto button : {quitApp, deleteProfile}) {
        button.set_style(R_BUTTON_FLAT); button.set_color(R_COLOR_TEXT, .94f, .59f, .62f, 1);
        button.set_color(R_COLOR_CONTROL_HOVERED, .43f, .16f, .22f, .28f);
    }
    behaviorCard.add_child(behaviorBox); preferencesPage.add_child(behaviorCard);
    auto startupOptionsCard = card(26); startupOptionsCard.slot_set_fill(1);
    auto startupOptionsBox = panel(R_WIDGET_VBOX, 14);
    auto startupHeading = label("General", 26, .94f, .96f, .98f); startupHeading.set_wrap(R_TRUE); startupOptionsBox.add_child(startupHeading);
    startupOptionsBox.add_child(label("Idioma", 12, .65f, .74f, .81f));
    auto languageChoice = raven::ComboBox::create();
    texts.add_item(languageChoice, "Español"); texts.add_item(languageChoice, "English");
    languageChoice.set_selected(preferences.language == i18n::Language::english ? 1 : 0);
    texts.accessible(languageChoice, "Idioma"); startupOptionsBox.add_child(languageChoice);
    auto autoStart = texts.checkbox("Iniciar con Windows"); autoStart.set_checked(startupEnabled.value_or(false)); startupOptionsBox.add_child(autoStart);
    texts.tooltip(autoStart, "Si mueves el programa, reactiva el inicio con Windows.");
    auto startInTray = texts.checkbox("Abrir en segundo plano"); startInTray.set_checked(preferences.startInTray); startupOptionsBox.add_child(startInTray);
    auto startupOptionsHint = label("El inicio con Windows abre en la bandeja.", 14, .71f, .78f, .83f); startupOptionsHint.set_wrap(R_TRUE); startupOptionsBox.add_child(startupOptionsHint);
    auto restoreLighting = texts.checkbox("Restaurar último efecto");
    restoreLighting.set_checked(preferences.restoreLighting); startupOptionsBox.add_child(restoreLighting);
    texts.tooltip(restoreLighting, "Reaplica automáticamente el último efecto enviado correctamente. No envía cambios pendientes.");
    auto startupOptionsSpace = raven::Widget::create(R_WIDGET_SPACER); startupOptionsSpace.slot_set_fill(1); startupOptionsBox.add_child(startupOptionsSpace);
    auto generalStatus = label(generalStore.readable() ? ""
        : "No se pudieron leer las preferencias. Los cambios no se guardarán.", 13, .62f, .70f, .78f); generalStatus.set_wrap(R_TRUE); startupOptionsBox.add_child(generalStatus);
    startupOptionsCard.add_child(startupOptionsBox); preferencesPage.add_child(startupOptionsCard); pages.add_child(preferencesPage);
    for (const auto field : {status, profileStatus, appearanceStatus, generalStatus, pollingPreferenceNote, receiverHint}) field.set_min_size(-1, 18);
    bool windowHasBeenShown = false;
    const auto saveGeneral = [&]() {
        if (test) return true;
        if (!preferences.rememberWindow) preferences.bounds.reset();
        else if (windowHasBeenShown && !hiddenToTray) {
            if (auto bounds = capture_window_bounds(nativeWindow)) preferences.bounds = bounds;
        }
        const bool saved = generalStore.save(preferences);
        texts.set(generalStatus, saved ? ""
            : "No se pudieron guardar las preferencias.");
        return saved;
    };
    const auto configureMediaClicks = [&](bool wanted, bool persist) {
        preferences.mediaClicks = wanted;
        mediaClicksActive = wanted && !test && mediaKnob.start();
        if (!wanted) mediaKnob.stop();
        remap.set_checked(wanted);
        texts.set(clickValues[0], mediaClicksActive ? "Reproducir / pausar" : "Silencio");
        texts.set(clickValues[1], mediaClicksActive ? "Siguiente pista" : "—");
        texts.set(clickValues[2], mediaClicksActive ? "Pista anterior" : "—");
        texts.set(knobStatus, wanted && !mediaClicksActive ? "No se pudo activar. Comprueba otras instancias."
            : mediaClicksActive ? "Activo" : "Desactivado");
        // Keep the requested preference after a transient hook failure, so a
        // later launch retries it. The checkbox remains available to disable it.
        if (persist) saveGeneral();
        if (refreshHome) refreshHome();
    };
    remap.set_changed_callback([&](Rid, int checked) { configureMediaClicks(checked != 0, true); });
    if (!test && preferences.mediaClicks) configureMediaClicks(true, false);
    const auto hideToTray = [&]() {
        if (!tray.available()) { texts.set(generalStatus, "La bandeja no está disponible."); return; }
        saveGeneral(); window.hide(); hiddenToTray = true;
    };
    const auto showWindow = [&]() {
        window.show();
        if (restoreMaximized) { window.maximize(); restoreMaximized = false; }
        else if (window.is_minimized()) window.restore();
        hiddenToTray = false; windowHasBeenShown = true;
        if (nativeWindow) SetForegroundWindow(nativeWindow);
    };
    hideNow.set_clicked_callback([&](Rid) { hideToTray(); });
    quitApp.set_clicked_callback([&](Rid) { exitRequested = true; });
    closeToTray.set_changed_callback([&](Rid, int checked) { preferences.closeToTray = checked != 0; saveGeneral(); });
    minimizeToTray.set_changed_callback([&](Rid, int checked) { preferences.minimizeToTray = checked != 0; saveGeneral(); });
    rememberWindow.set_changed_callback([&](Rid, int checked) { preferences.rememberWindow = checked != 0; saveGeneral(); });
    startInTray.set_changed_callback([&](Rid, int checked) { preferences.startInTray = checked != 0; saveGeneral(); });
    restoreLighting.set_changed_callback([&](Rid, int checked) {
        preferences.restoreLighting = checked != 0;
        if (checked) lightingRestore = LightingRestorePolicy{};
        saveGeneral();
    });
    autoStart.set_changed_callback([&](Rid, int checked) {
        if (test) return;
        const bool changed = startup.set(checked != 0);
        const auto enabled = startup.enabled(); autoStart.set_checked(enabled.value_or(false));
        texts.set(generalStatus, changed && enabled && *enabled == (checked != 0)
            ? (checked ? "" : "")
            : "No se pudo cambiar el inicio con Windows.");
    });
    ui::AmbientBackground ambientBackground;
    refreshBackground = [&]() {
        ambientBackground.update(window, appearance.atmosphere, previewR, previewG, previewB,
            previewMulticolor || lighting_uses_builtin_palette(previewMode), appearance.skin);
    };
    auto applyAppearance = [&]() {
        const bool glass = appearance.skin == ui::Skin::glass;
        ui::apply_theme(appearance.skin);
        for (const auto& surface : surfaces) surface.set_skin(appearance.skin);
        for (const auto& item : navigation) item.set_skin(appearance.skin);
        for (const auto& style : labelStyles) {
            // Keep headings, body copy and captions distinct in both materials.
            // Semantic status colors continue to be updated by their own state.
            if (style.red < .8f && style.blue - style.red < .25f) {
                const bool body = style.red >= .70f;
                style.widget.set_color(body ? .76f : glass ? .67f : .62f,
                    body ? .82f : glass ? .75f : .70f, body ? .89f : glass ? .84f : .80f, 1);
            }
        }
        stage.set_material(glass ? R_MATERIAL_FROSTED_GLASS : R_MATERIAL_SOLID);
        stage.set_background(glass ? .08f : .11f, glass ? .11f : .127f, glass ? .16f : .15f, glass ? .22f : 1);
        selectionCard.set_material(glass ? R_MATERIAL_FROSTED_GLASS : R_MATERIAL_SOLID);
        selectionCard.set_background(glass ? .06f : .105f, glass ? .085f : .12f, glass ? .13f : .15f, glass ? .28f : 1);
        colorChip.set_material(R_MATERIAL_SOLID);
        for (const auto& swatch : swatches) swatch.set_material(R_MATERIAL_SOLID);
        atmosphereChoice.set_enabled(glass);
        texts.set(materialTitle, "Vista del tema");
        refreshBackground(); materialDemo.invalidate();
    };
    applyAppearance();
    const auto appearanceChanged = [&]() {
        appearance.skin = skinChoice.get_selected() == 0 ? ui::Skin::glass : ui::Skin::neumorphic;
        appearance.atmosphere = atmosphereChoice.get_selected() == 0 ? ui::Atmosphere::aurora : ui::Atmosphere::keyboard;
        applyAppearance();
        if (!test) texts.set(appearanceStatus, ui::save_appearance(appearancePath, appearance)
            ? ""
            : "No se pudo guardar la apariencia.");
    };
    skinChoice.set_changed_callback([&](Rid, int) { appearanceChanged(); });
    atmosphereChoice.set_changed_callback([&](Rid, int) { appearanceChanged(); });
    const std::array<raven::Widget, 7> pageWidgets{{content, knobCard, profilesPage, devicePage, homePage, appearancePage, preferencesPage}};
    for (const auto& page : pageWidgets) page.set_transition(R_TRANSITION_NONE, 0);
    navigate = [&](int index) {
        if (index < 0 || index >= static_cast<int>(pageWidgets.size())) return;
        const int previous = pages.get_index();
        const bool changed = previous != index;
        pages.set_index(index); texts.set(title, pageNames[index]);
        for (std::size_t i = 0; i < navigation.size(); ++i) {
            navigation[i].select(static_cast<int>(i) == index);
            pageWidgets[i].set_accessible_hidden(static_cast<int>(i) != index);
        }
        if (changed && !test) {
            const auto before = std::find(navigationOrder.begin(), navigationOrder.end(), previous);
            const auto after = std::find(navigationOrder.begin(), navigationOrder.end(), index);
            pageWidgets[index].play_transition(after > before ? R_TRANSITION_SLIDE_LEFT : R_TRANSITION_SLIDE_RIGHT, 0);
        }
    };
    for (std::size_t i = 0; i < navigation.size(); ++i)
        navigation[i].button.set_clicked_callback([&, i](Rid) { navigate(static_cast<int>(i)); });
    navigate(homePageIndex); root.add_child(pages);
    auto responsiveLayout = [&](int width, int) {
        const bool compact = width < 1240;
        sidebar.set_size(compact ? 72.0f : 204.0f, -1);
        shell.set_spacing(compact ? 18.0f : 28.0f);
        const float margin = compact ? 20.0f : 28.0f; shell.slot_set_padding(margin, margin, margin, margin);
        texts.set(logo, compact ? "A" : "AULA Control");
        sidebarDevice.set_visibility(compact ? R_COLLAPSED : R_VISIBLE);
        for (const auto& item : navigation) {
            item.text.set_visibility(compact ? R_COLLAPSED : R_VISIBLE);
            item.button.set_metric(R_METRIC_PADDING_X, compact ? 4.0f : 12.0f);
        }
        settingsCard.set_size(compact ? 320.0f : 360.0f, -1);
        homeSummary.set_size(compact ? 300.0f : 360.0f, -1);
        batteryCard.set_size(compact ? 220.0f : 240.0f, -1);
    };
    window.set_resize_callback([&](Rid, int width, int height) { responsiveLayout(width, height); });
    auto appViews = raven::Switcher::create(); appViews.add_child(startupPage); appViews.add_child(shell);
    shell.set_transition(R_TRANSITION_FADE, 0); shell.set_accessible_hidden(R_TRUE);
    const auto enterMain = [&]() { appViews.set_index(1); startupPage.set_accessible_hidden(R_TRUE); shell.set_accessible_hidden(R_FALSE); };
    enterApp.set_clicked_callback([&](Rid) { enterMain(); });
    int initialWidth = 1440, initialHeight = 900; window.get_size(&initialWidth, &initialHeight);
    responsiveLayout(initialWidth, initialHeight); window.set_content(appViews);
    const auto folder = std::filesystem::path(executablePath).parent_path().wstring() + L"\\";
    enum class Job { receiver, battery, lighting };
    std::future<ToolResult> task; bool running = false; Job job = Job::receiver;
    unsigned queryGeneration = 0;
    const auto cancelTools = std::make_shared<std::atomic_bool>(false);
    window.set_close_callback([&](Rid) {
        if (preferences.closeToTray && tray.available() && !tray.ending_session()) hideToTray();
        else exitRequested = true;
        return R_FALSE;
    });
    auto renderBattery = [&]() {
        const auto now = BatteryState::Clock::now();
        const auto phase = batteryState.phase(now);
        const auto devicePhase = deviceState.phase();
        const bool usb = deviceState.transport() == ControlTransport::usb && deviceState.presence() == DeviceState::Presence::present;
        const auto percent = usb ? std::optional<int>{} : batteryState.percent();
        const auto age = usb ? std::optional<long long>{} : batteryState.age(now);
        const bool stale = !usb && (phase == BatteryState::Phase::stale || phase == BatteryState::Phase::unavailable
            || (batteryState.percent() && devicePhase != DeviceState::Phase::responding));
        const auto ageText = age ? (*age < 60 ? std::to_string(*age) + " s" : std::to_string(*age / 60) + " min") : std::string{};
        const auto value = usb ? "USB" : percent ? std::to_string(*percent) + "%" : "--";
        texts.set(batteryValue, value.c_str()); texts.set(deviceBattery, value.c_str());
        batteryBar.set_value(percent.value_or(0) / 100.0f); deviceBar.set_value(percent.value_or(0) / 100.0f);
        batteryBar.set_visibility(usb ? R_COLLAPSED : R_VISIBLE); deviceBar.set_visibility(usb ? R_COLLAPSED : R_VISIBLE);
        pollingInterval.set_enabled(!usb);
        texts.set(homeConnection, usb ? "USB" : "2.4 GHz");
        texts.set(deviceBatteryTitle, usb ? "CONEXIÓN  /  USB" : "BATERÍA  /  RECEPTOR 2.4 GHz");
        const bool low = percent && *percent <= 20;
        const float red = stale ? 0.95f : low ? 1.0f : 0.16f;
        const float green = stale ? 0.72f : low ? 0.38f : 0.76f;
        const float blue = stale ? 0.35f : low ? 0.38f : 0.88f;
        batteryBar.set_color(R_COLOR_ACCENT, red, green, blue, 1); deviceBar.set_color(R_COLOR_ACCENT, red, green, blue, 1);
        batteryValue.set_color(stale ? red : 0.93f, stale ? green : 0.96f, stale ? blue : 0.98f, 1);
        const char* state = "Comprobando teclado...";
        const char* hint = "Comprobando teclado...";
        using DevicePhase = DeviceState::Phase;
        switch (devicePhase) {
        case DevicePhase::receiver_missing:
            state = "Teclado no detectado";
            hint = "Conecta el cable en modo USB o el receptor 2.4 GHz.";
            break;
        case DevicePhase::responding:
            state = usb ? "Conectado por USB" : stale ? "Lectura anterior · consulta pendiente" : "Última comunicación correcta";
            hint = "";
            break;
        case DevicePhase::no_response:
            state = "Teclado sin respuesta";
            hint = usb ? "Comprueba el cable y el modo USB del teclado." : "Comprueba que el teclado esté en modo 2.4 GHz.";
            break;
        case DevicePhase::ambiguous:
            state = "Varios dispositivos compatibles";
            hint = "Conecta un solo teclado o receptor compatible.";
            break;
        case DevicePhase::busy:
            state = "Canal de control no accesible";
            hint = "Comprueba que otro programa no esté usando el teclado.";
            break;
        case DevicePhase::incompatible:
            state = "Canal de control sin verificar";
            hint = "Vuelve a conectar el teclado o receptor.";
            break;
        case DevicePhase::error:
            state = "No se pudo comprobar el teclado";
            hint = "Revisa la conexión y los archivos de la aplicación.";
            break;
        default:
            if (deviceState.presence() != DeviceState::Presence::present) {
                state = "Buscando teclado...";
                hint = "Buscando teclado...";
            }
            break;
        }
        texts.set(deviceStatus, state); texts.set(receiverHint, hint); texts.set(connectionLabel, state);
        if (!test) tray.update(texts.translate(state) + (percent ? " · " + value + (stale ? texts.translate(" (anterior)") : "") : ""));
        const bool healthy = devicePhase == DevicePhase::responding && (usb || !stale);
        const bool checking = devicePhase == DevicePhase::checking;
        connectionRed = healthy ? .20f : checking ? .65f : .95f;
        connectionGreen = healthy ? .80f : checking ? .74f : .72f;
        connectionBlue = healthy ? .90f : checking ? .81f : .35f;
        deviceStatus.set_color(connectionRed, connectionGreen, connectionBlue, 1); connectionDot.invalidate();
        texts.set(startupMessage, state);
        apply.set_enabled(!running && deviceState.can_apply());
        texts.set(apply, running && job == Job::lighting ? "Aplicando…" : "Aplicar al teclado");
        if (deviceState.settled() && appViews.get_index() == 0) enterMain();
        texts.set(batteryTitle, usb ? "CONEXIÓN  /  USB" : devicePhase == DevicePhase::receiver_missing ? "BATERÍA  /  SIN RECEPTOR"
            : phase == BatteryState::Phase::unavailable ? "BATERÍA  /  SIN RESPUESTA"
            : stale ? "BATERÍA  /  LECTURA ANTERIOR" : "BATERÍA  /  2.4 GHz");
        const auto detail = usb ? std::string("Batería no disponible por USB.") : age ? "Lectura hace " + ageText + (phase == BatteryState::Phase::updating ? " · actualizando" : "")
            : phase == BatteryState::Phase::updating ? "Consultando receptor..." : "Sin lectura válida";
        texts.set(batteryDetail, detail.c_str());
        const auto last = usb ? std::string("Batería no disponible por USB.") : age ? "Última lectura: hace " + ageText + "." : "Sin lectura válida";
        texts.set(lastReading, last.c_str());
        const auto next = usb ? std::string("Batería automática solo por 2.4 GHz.") : deviceState.presence() != DeviceState::Presence::present ? std::string("Detección de conexión cada 5 s.")
            : phase == BatteryState::Phase::updating ? std::string("Consulta en curso...")
            : batteryState.due(now) && running ? std::string("Consulta pendiente.")
            : "Próxima consulta en " + std::to_string(batteryState.remaining(now)) + " s.";
        texts.set(nextReading, next.c_str());
    };
    pollingInterval.set_changed_callback([&](Rid, int index) {
        preferences.batteryInterval = index == 0 ? 30 : 60;
        batteryState.set_interval(preferences.batteryInterval, BatteryState::Clock::now()); renderBattery();
        texts.set(pollingPreferenceNote, saveGeneral() ? "" : "No se pudo guardar el intervalo.");
    });
    const auto changeLanguage = [&](int index) {
        preferences.language = index == 1 ? i18n::Language::english : i18n::Language::spanish;
        texts.set_language(preferences.language); tray.set_language(preferences.language);
        renderBattery(); saveGeneral();
    };
    languageChoice.set_changed_callback([&](Rid, int index) { changeLanguage(index); });
    renderBattery();
    const auto sendLighting = [&](const LightingSettings& s, bool automatic) {
        if (running || !deviceState.can_apply() || !s.valid()) return;
        sendingSettings = s;
        sendingTransport = deviceState.transport(); sendingGeneration = deviceState.generation();
        automaticLightingJob = automatic; lightingRestore.claim(sendingGeneration);
        const auto args = lighting_tool_arguments(s) + L" --transporte "
            + (deviceState.transport() == ControlTransport::usb ? L"usb" : L"2g");
        texts.set(status, automatic ? "Restaurando iluminación…" : "Enviando ajustes…"); status.set_color(.58f, .84f, .91f, 1);
        texts.set(apply, "Aplicando…"); apply.set_enabled(R_FALSE); job = Job::lighting; running = true;
        task = std::async(std::launch::async, run_tool, folder + L"rgb_prueba.exe", args, cancelTools, std::chrono::milliseconds(8000));
    };
    apply.set_clicked_callback([&](Rid) { sendLighting(readDraft(), false); });
    if (test) {
        window.show();
        raven::draw_frame();
        float startX, startY, startW, startH; startupCard.get_rect(&startX, &startY, &startW, &startH);
        const bool validStartup = appViews.get_index() == 0 && startW > 0 && startH > 0 && startY + startH <= 900;
        enterMain();
        window.set_size(900, 760); responsiveLayout(900, 760); navigate(0); raven::draw_frame();
        float x, y, width, height; keyboard.get_rect(&x, &y, &width, &height);
        bool ok = validStartup && window.is_valid() && apply.is_valid() && batteryValue.is_valid() && brightness.is_valid() && effect.is_valid() && picker != R_INVALID_ID
            && width > 0 && height > 0 && y + height <= 760 && !recoverLighting.is_enabled();
        status.get_rect(&x, &y, &width, &height); ok = ok && y + height <= 760;
        navigate(1); raven::draw_frame();
        knobStatus.get_rect(&x, &y, &width, &height);
        ok = ok && remap.is_valid() && !remap.is_checked() && width > 0 && height > 0 && y + height <= 760;
        navigate(2); raven::draw_frame();
        profileStatus.get_rect(&x, &y, &width, &height);
        ok = ok && saveProfile.is_valid() && profileName.is_valid() && profiles.entries().empty()
            && width > 0 && height > 0 && y + height <= 760;
        const LightingSettings sample{12, 63, 240, 19, 25, true, 5, 2};
        restoreDraft(sample); ok = ok && readDraft() == sample;
        ok = ok && speed.is_enabled() && nativeBrightness.is_enabled() && !brightness.is_enabled() && !raven::Widget(picker).is_enabled()
            && std::lround(previewPower * 100) == 40;
        nativeBrightness.set_value(4); speed.set_value(1); brightness.set_value(50); synchronizeSliders();
        ok = ok && readDraft() == LightingSettings{12, 63, 240, 19, 50, true, 1, 4}
            && std::string(nativeBrightnessLabel.get_text()) == "BRILLO DEL TECLADO   4 / 5"
            && std::string(brightnessLabel.get_text()) == "AJUSTE RGB   50%" && previewSpeed == 1;
        restoreDraft({255, 0, 0, 6, 25, false, 5, 1});
        ok = ok && !speed.is_enabled() && nativeBrightness.is_enabled() && !brightness.is_enabled() && std::lround(previewPower * 100) == 20;
        restoreDraft({255, 0, 0, 8, 25, false, 1, 5});
        ok = ok && speed.is_enabled() && nativeBrightness.is_enabled() && !brightness.is_enabled() && previewSpeed == 1 && previewPower == 1.0f;
        restoreDraft({255, 0, 0, 1, 50, false, 5, 4});
        ok = ok && !speed.is_enabled() && brightness.is_enabled() && std::lround(previewPower * 100) == 40;
        restoreDraft({255, 0, 0, 7, 50, false});
        ok = ok && !previewMulticolor && previewMode == 7 && std::lround(previewPower * 100) == 30;
        lastSent = sample; refreshDraft(); recoverLastLighting();
        ok = ok && readDraft() == sample && recoverLighting.is_enabled();
        const LightingSettings unsent{240, 12, 8, 7, 70, false, 1, 4};
        restoreDraft(unsent);
        ok = ok && readDraft() == unsent && !draftMatchesSent && *lastSent == sample
            && std::string(homeEffect.get_text()) == texts.translate(effectName(sample))
            && std::string(homeColor.get_text()) == texts.translate(colorDescription(sample));
        recoverLastLighting(); ok = ok && readDraft() == sample && draftMatchesSent;
        lastSent.reset(); refreshDraft();
        navigate(3); raven::draw_frame();
        receiverHint.get_rect(&x, &y, &width, &height);
        ok = ok && width > 0 && height > 0 && y + height <= 760;
        deviceState.finish_probe(DeviceReply{DeviceCode::present, {}}, DeviceState::Clock::now());
        deviceState.finish_query(DeviceReply{DeviceCode::ready, 95}, deviceState.start_query());
        batteryState.finish(95, BatteryState::Clock::now());
        deviceState.finish_probe(DeviceReply{DeviceCode::absent, {}}, DeviceState::Clock::now()); batteryState.invalidate(); renderBattery();
        ok = ok && std::string(batteryValue.get_text()) == "95%" && std::string(deviceStatus.get_text()) == "Teclado no detectado" && !apply.is_enabled();
        deviceState.finish_probe(DeviceReply{DeviceCode::present, {}}, DeviceState::Clock::now());
        deviceState.finish_query(DeviceReply{DeviceCode::ready, 0}, deviceState.start_query()); batteryState.finish(0, BatteryState::Clock::now()); renderBattery();
        ok = ok && std::string(deviceBattery.get_text()) == "0%" && batteryBar.get_value() == 0;
        const auto contained = [](raven::Widget widget, int w, int h) {
            float left, top, width, height; widget.get_rect(&left, &top, &width, &height);
            return width > 0 && height > 0 && left >= 0 && top >= 0 && left + width <= w + 1 && top + height <= h + 1;
        };
        const auto unchangedDraft = readDraft();
        const bool animationsWereEnabled = raven::get_animations_enabled() != 0;
        raven::set_animations_enabled(R_FALSE);
        const auto frozenPreview = previewTime();
        ok = ok && previewTime() == frozenPreview && readDraft() == unchangedDraft;
        raven::set_animations_enabled(animationsWereEnabled);
        for (const auto language : {i18n::Language::spanish, i18n::Language::english}) {
        changeLanguage(language == i18n::Language::english ? 1 : 0);
        languageChoice.set_selected(language == i18n::Language::english ? 1 : 0);
        ok = ok && readDraft() == unchangedDraft && !remap.is_checked()
            && std::string(selectedTitle.get_text()) == texts.translate("PALETA DEL EFECTO")
            && std::string(homeDraftState.get_text()) == (lastSent ? texts.translate(texts.source(editState)) : "");
        for (const auto skin : {ui::Skin::glass, ui::Skin::neumorphic}) {
        appearance.skin = skin; applyAppearance();
        ok = ok && readDraft() == unchangedDraft && !remap.is_checked();
        for (const auto& size : std::array<std::array<int, 2>, 4>{{{900, 760}, {1120, 800}, {1440, 900}, {1920, 1080}}}) {
            const int w = size[0], h = size[1]; window.set_size(w, h); responsiveLayout(w, h);
            appViews.set_index(0); startupPage.set_accessible_hidden(R_FALSE); shell.set_accessible_hidden(R_TRUE); raven::draw_frame();
            ok = ok && contained(startupCard, w, h) && contained(enterApp, w, h);
            enterMain();
            for (int index = 0; index < static_cast<int>(pageWidgets.size()); ++index) {
                navigate(index); raven::draw_frame();
                ok = ok && contained(pageWidgets[index], w, h) && contained(navigation[index].button, w, h)
                    && std::string(title.get_text()) == texts.translate(pageNames[index]) && *navigation[index].selected;
                if (index == 0) {
                    ok = ok && contained(keyboard, w, h) && contained(apply, w, h) && contained(status, w, h) && contained(swatches.back(), w, h)
                        && contained(speed, w, h) && (lighting_uses_builtin_palette(previewMode) ? contained(note, w, h) : true) && contained(nativeBrightness, w, h);
                    keyboard.get_rect(&x, &y, &width, &height); ok = ok && (w < 1440 || width >= 600);
                    const auto saved = readDraft();
                    for (const auto& variant : std::array<LightingSettings, 3>{{{255, 0, 0, 11, 50, false, 5, 4}, {255, 0, 0, 11, 50, true, 1, 2}, {255, 0, 0, 8, 25, false, 3, 1}}}) {
                        restoreDraft(variant); raven::draw_frame();
                        ok = ok && readDraft() == variant && nativeBrightness.is_enabled() && contained(nativeBrightness, w, h)
                            && contained(speed, w, h) && (lighting_uses_builtin_palette(previewMode) ? contained(note, w, h) : true) && contained(status, w, h) && contained(apply, w, h);
                        if (!variant.multicolor && !lighting_uses_builtin_palette(variant.mode))
                            ok = ok && contained(brightness, w, h) && contained(brightnessLabel, w, h);
                    }
                    restoreDraft(saved); raven::draw_frame();
                } else if (index == 1) {
                    // Exercise both sets of translated action labels without
                    // installing a global hook or changing user preferences.
                    for (const bool active : {true, false}) {
                        texts.set(clickValues[0], active ? "Reproducir / pausar" : "Silencio");
                        texts.set(clickValues[1], active ? "Siguiente pista" : "—");
                        texts.set(clickValues[2], active ? "Pista anterior" : "—");
                        raven::draw_frame();
                        ok = ok && contained(knobStatus, w, h) && contained(remap, w, h)
                            && contained(remapScope, w, h) && contained(remapMemory, w, h);
                        for (const auto value : clickValues) ok = ok && contained(value, w, h);
                    }
                }
                else if (index == 2) ok = ok && contained(undoDelete, w, h) && contained(profileStatus, w, h);
                else if (index == 3) {
                    for (const auto transport : {ControlTransport::receiver, ControlTransport::usb}) {
                    for (const auto code : {DeviceCode::absent, DeviceCode::no_response, DeviceCode::busy, DeviceCode::incompatible, DeviceCode::ambiguous, DeviceCode::error, DeviceCode::ready}) {
                        deviceState.finish_probe(DeviceReply{DeviceCode::present, {}, transport}, DeviceState::Clock::now());
                        deviceState.finish_query(DeviceReply{code, code == DeviceCode::ready && transport == ControlTransport::receiver ? std::optional<int>(0) : std::optional<int>{}, transport}, deviceState.start_query());
                        renderBattery(); raven::draw_frame();
                        ok = ok && contained(receiverHint, w, h) && contained(nextReading, w, h) && contained(connectionLabel, w, h);
                        if (transport == ControlTransport::usb && code == DeviceCode::ready)
                            ok = ok && deviceState.can_apply() && !pollingInterval.is_enabled()
                                && std::string(batteryValue.get_text()) == "USB"
                                && std::string(batteryDetail.get_text()) == texts.translate("Batería no disponible por USB.");
                    }
                    }
                    deviceState.finish_probe(DeviceReply{DeviceCode::present, {}}, DeviceState::Clock::now());
                    deviceState.finish_query(DeviceReply{DeviceCode::ready, 0}, deviceState.start_query()); renderBattery();
                }
                else if (index == homePageIndex) ok = ok && contained(homeKeyboard, w, h) && contained(homeLightingButton, w, h)
                    && contained(homeDeviceButton, w, h) && contained(homeKnobState, w, h);
                else if (index == 5) ok = ok && contained(skinChoice, w, h) && contained(atmosphereChoice, w, h) && contained(appearanceStatus, w, h);
                else ok = ok && contained(closeToTray, w, h) && contained(minimizeToTray, w, h) && contained(rememberWindow, w, h)
                    && contained(autoStart, w, h) && contained(startInTray, w, h) && contained(hideNow, w, h)
                    && contained(quitApp, w, h) && contained(generalStatus, w, h) && contained(languageChoice, w, h) && contained(restoreLighting, w, h);
            }
        }
        }
        }
        std::istringstream savedAppearance("AULA_APPEARANCE 1\nneumorphic keyboard\n");
        auto parsedAppearance = ui::read_appearance(savedAppearance);
        ok = ok && parsedAppearance.skin == ui::Skin::neumorphic && parsedAppearance.atmosphere == ui::Atmosphere::keyboard;
        std::istringstream invalidAppearance("AULA_APPEARANCE 1\nglass unknown\n");
        ok = ok && ui::read_appearance(invalidAppearance).atmosphere == ui::Atmosphere::aurora;
        ok = ok && closeToTray.is_checked() && !minimizeToTray.is_checked() && rememberWindow.is_checked()
            && !autoStart.is_checked() && !startInTray.is_checked() && !remap.is_checked();
        const auto missing = texts.missing_translations();
        for (const auto& source : missing) std::fprintf(stderr, "Missing translation: %s\n", source.c_str());
        ok = ok && missing.empty();
        raven::shutdown(); return ok ? 0 : 1;
    }
    unsigned lastPresses = 0, lastFailures = 0;
    auto nextBatteryPaint = BatteryState::Clock::now();
    tray.create();
    if (!tray.available()) texts.set(generalStatus, "No se pudo crear el icono de la bandeja. Cerrar termina la aplicación.");
    else if (!startupEnabled) texts.set(generalStatus, "No se pudo comprobar el inicio con Windows.");
    hideNow.set_enabled(tray.available());
    if ((backgroundLaunch || preferences.startInTray) && tray.available()) hiddenToTray = true;
    else showWindow();
    auto nextTrayRetry = BatteryState::Clock::now() + std::chrono::seconds(5);
    PreviewCadence previewCadence;
    bool timelineAnimationsEnabled = raven::get_animations_enabled() != 0;
    const auto customAnimationVisible = [&]() {
        if (!raven::get_animations_enabled() || hiddenToTray || window.is_minimized()) return false;
        if (appViews.get_index() == 0) return true;
        return pages.get_index() == 0 ? lighting_has_speed(previewMode)
            : pages.get_index() == homePageIndex && lastSent && lighting_has_speed(lastSent->mode);
    };
    while (raven::wait_frame(previewCadence.wait_seconds(customAnimationVisible(), hiddenToTray || window.is_minimized(), PreviewCadence::Clock::now()))) {
        const bool animationsEnabled = raven::get_animations_enabled() != 0;
        if (animationsEnabled != timelineAnimationsEnabled) {
            const auto tick = LightingTimeline::Clock::now();
            draftTimeline.sample(tick, timelineAnimationsEnabled); appliedTimeline.sample(tick, timelineAnimationsEnabled);
            timelineAnimationsEnabled = animationsEnabled;
        }
        const auto trayAction = tray.take_action();
        if (instance.restore_requested() || trayAction == DesktopTray::Action::show) showWindow();
        if (trayAction == DesktopTray::Action::preferences) { enterMain(); navigate(preferencesPageIndex); showWindow(); }
        if (trayAction == DesktopTray::Action::exit || tray.ending_session()) exitRequested = true;
        if (exitRequested) { saveGeneral(); cancelTools->store(true); mediaKnob.stop(); raven::quit(0); break; }
        if (!tray.available()) {
            if (hiddenToTray) { showWindow(); texts.set(generalStatus, "La bandeja no está disponible. Se restauró la ventana."); }
            if (BatteryState::Clock::now() >= nextTrayRetry) { tray.retry(); nextTrayRetry = BatteryState::Clock::now() + std::chrono::seconds(5); }
        }
        hideNow.set_enabled(tray.available());
        if (!hiddenToTray && preferences.minimizeToTray && window.is_minimized() && tray.available()) hideToTray();
        synchronizeSliders();
        const auto mediaFeedback = mediaKnob.feedback();
        const unsigned presses = mediaFeedback.actions, failures = mediaFeedback.failures;
        if (presses != lastPresses || failures != lastFailures) {
            const bool failed = failures != lastFailures;
            lastPresses = presses; lastFailures = failures;
            const auto text = failed ? "No se pudo enviar la acción multimedia."
                : mediaFeedback.lastAction == MediaAction::next_track ? "Siguiente pista enviada."
                : mediaFeedback.lastAction == MediaAction::previous_track ? "Pista anterior enviada."
                : "Reproducir / pausar enviado.";
            if (mediaClicksActive) texts.set(knobStatus, text);
        }
        if (previewCadence.due(customAnimationVisible(), PreviewCadence::Clock::now())) {
            if (appViews.get_index() == 0) startupSpinner.invalidate();
            else if (pages.get_index() == 0) keyboard.invalidate();
            else if (pages.get_index() == homePageIndex) homeKeyboard.invalidate();
        }
        const auto now = BatteryState::Clock::now();
        if (!running && deviceState.probe_due(now)) {
            deviceState.start_probe(); job = Job::receiver; running = true; renderBattery();
            task = std::async(std::launch::async, run_tool, folder + L"bateria_f75.exe", L"--detectar", cancelTools, std::chrono::milliseconds(8000));
        } else if (!running && lightingRestore.due(preferences.restoreLighting, lastSent.has_value(),
                deviceState.phase() == DeviceState::Phase::responding && deviceState.can_apply(), deviceState.generation())) {
            sendLighting(*lastSent, true);
        } else if (!running && deviceState.presence() == DeviceState::Presence::present
            && (deviceState.query_requested() || (deviceState.transport() == ControlTransport::receiver && batteryState.due(now)))) {
            queryGeneration = deviceState.start_query(); batteryState.start();
            job = Job::battery; running = true; renderBattery();
            const auto arguments = deviceState.transport() == ControlTransport::usb ? L"--estado --transporte usb" : L"--estado --transporte 2g";
            task = std::async(std::launch::async, run_tool, folder + L"bateria_f75.exe", arguments, cancelTools, std::chrono::milliseconds(8000));
        }
        if (running && task.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto result = task.get(); running = false;
            const auto reply = result.ok ? device_reply(result.output) : std::optional<DeviceReply>{};
            if (job == Job::receiver) {
                const auto previousGeneration = deviceState.generation();
                const auto previousTransport = deviceState.transport();
                deviceState.finish_probe(reply, BatteryState::Clock::now());
                if (deviceState.presence() != DeviceState::Presence::present || previousTransport != deviceState.transport()) batteryState.invalidate();
                if (deviceState.query_requested()) batteryState.request_now(BatteryState::Clock::now());
                if (previousGeneration != deviceState.generation()) refreshDraft();
                renderBattery();
            } else if (job == Job::battery) {
                const bool accepted = deviceState.finish_query(reply, queryGeneration);
                batteryState.finish(accepted && reply && reply->code == DeviceCode::ready ? reply->percent : std::optional<int>{}, BatteryState::Clock::now());
                if (accepted && reply && reply->code == DeviceCode::no_response) {
                    // The dongle can stay plugged in while the keyboard powers
                    // off. Restore once when radio communication recovers too.
                    lightingRestore = LightingRestorePolicy{}; appliedGeneration.reset(); refreshDraft();
                }
                renderBattery();
            }
            else if (!result.ok) {
                texts.set(status, result.timedOut ? "El envío tardó demasiado. Revisa la conexión." : "No se pudo comunicar. Revisa la conexión.");
                status.set_color(.95f, .72f, .35f, 1);
            }
            else {
                const auto receipt = lighting_receipt(result.output);
                if (!receipt || receipt->transport != sendingTransport || !sendingSettings || !sendingSettings->valid()) {
                    texts.set(status, "No se pudo confirmar el envío de iluminación."); status.set_color(.95f, .72f, .35f, 1);
                } else {
                    lastSent = sendingSettings; appliedGeneration = sendingGeneration;
                    appliedTimeline.restart(lastSent->speed, lighting_elapsed_seconds(receipt->timeMs, lighting_wall_time_ms()), LightingTimeline::Clock::now());
                    const bool saved = lastLightingStore.save(LastLighting{*lastSent, *receipt});
                    refreshDraft(); keyboard.invalidate(); homeKeyboard.invalidate();
                    texts.set(status, !saved ? "Ajustes enviados. No se pudo guardar el último efecto."
                        : automaticLightingJob ? "Iluminación restaurada."
                        : readDraft() == *lastSent ? "Ajustes enviados." : "Envío completado. Hay cambios pendientes.");
                    status.set_color(saved ? .54f : .95f, saved ? .82f : .72f, saved ? .77f : .35f, 1);
                }
            }
            if (job == Job::lighting) renderBattery();
        }
        if (BatteryState::Clock::now() >= nextBatteryPaint) {
            renderBattery(); nextBatteryPaint = BatteryState::Clock::now() + std::chrono::seconds(1);
        }
    }
    cancelTools->store(true);
    mediaKnob.stop();
    if (running) task.wait(); raven::shutdown();
}
