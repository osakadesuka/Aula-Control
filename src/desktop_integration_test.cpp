#include "desktop_integration.hpp"
#include <cstdlib>
#include <iostream>
#include <sstream>

static void require(bool value, const char* reason) {
    if (!value) { std::cerr << reason << '\n'; std::exit(1); }
}
// Manual UI regression surface: exercises the production tray menu without
// changing preferences, registering startup or accessing the keyboard.
static int interactive_menu_check() {
    struct Check { DesktopTray tray; HWND status = nullptr; unsigned cancelled = 0; } check;
    WNDCLASSW type{}; type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = L"AulaControl.TrayMenuCheck";
    type.hbrBackground = GetSysColorBrush(COLOR_WINDOW); type.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    type.lpfnWndProc = [](HWND window, UINT message, WPARAM wparam, LPARAM lparam) -> LRESULT {
        auto state = reinterpret_cast<Check*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            state = static_cast<Check*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        }
        if (message == WM_CREATE) {
            CreateWindowW(L"BUTTON", L"Abrir menú de bandeja", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 20, 230, 38, window, reinterpret_cast<HMENU>(100), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"Fuera del menú", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 90, 230, 38, window, reinterpret_cast<HMENU>(101), nullptr, nullptr);
            state->status = CreateWindowW(L"STATIC", L"Listo para comprobar", WS_CHILD | WS_VISIBLE, 20, 155, 530, 55, window, nullptr, nullptr, nullptr);
            return 0;
        }
        if (message == WM_COMMAND && LOWORD(wparam) == 100) {
            POINT point{280, 35}; ClientToScreen(window, &point); state->tray.show_context_menu(point);
            const auto action = state->tray.take_action();
            std::wstring result;
            if (action == DesktopTray::Action::none) result = L"Menú cerrado sin ejecutar · " + std::to_wstring(++state->cancelled);
            else if (action == DesktopTray::Action::show) result = L"Abrir seleccionado";
            else if (action == DesktopTray::Action::preferences) result = L"Preferencias seleccionado";
            else result = L"Salir seleccionado";
            SetWindowTextW(state->status, result.c_str()); return 0;
        }
        if (message == WM_COMMAND && LOWORD(wparam) == 101) return 0;
        if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
        return DefWindowProcW(window, message, wparam, lparam);
    };
    if (!RegisterClassW(&type)) return 1;
    if (!check.tray.create()) return 1;
    const auto window = CreateWindowW(type.lpszClassName, L"AULA Tray Menu Check", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 310, nullptr, nullptr, type.hInstance, &check);
    if (!window) return 1;
    ShowWindow(window, SW_SHOW); MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return 0;
}
int main(int argc, char** argv) {
#ifdef AULA_MENU_CHECK
    return interactive_menu_check();
#endif
    if (argc > 1 && std::string(argv[1]) == "--menu") return interactive_menu_check();
    for (const auto language : {i18n::Language::spanish, i18n::Language::english}) {
    for (const UINT dpi : {96u, 144u, 192u}) {
        TrayMenu menu(dpi, true, language);
        require(menu.handle() && menu.custom() && GetMenuItemCount(menu.handle()) == 4, "Create dark menu at common display scales");
        for (UINT i = 0; i < menu.items().size(); ++i) {
            const auto& entry = menu.items()[i];
            MENUITEMINFOW info{sizeof(MENUITEMINFOW)}; wchar_t text[64]{};
            info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA | MIIM_STRING; info.dwTypeData = text; info.cch = 64;
            require(GetMenuItemInfoW(menu.handle(), i, TRUE, &info) && info.wID == entry.command
                && (info.fType & MFT_OWNERDRAW), "Keep native menu IDs and owner-draw metadata");
            if (!entry.separator) require(std::wstring(text) == entry.text, "Owner drawing retains native accessible text");
            MEASUREITEMSTRUCT measure{}; measure.CtlType = ODT_MENU; measure.itemData = info.dwItemData;
            require(menu.measure(measure) && measure.itemWidth >= static_cast<UINT>(MulDiv(220, dpi, 96))
                && measure.itemHeight >= static_cast<UINT>(MulDiv(entry.separator ? 9 : 34, dpi, 96)), "Menu text and separators fit at each DPI");
        }
    }
    }
    TrayMenu accessible(96, false);
    MENUITEMINFOW native{sizeof(MENUITEMINFOW)}; native.fMask = MIIM_FTYPE;
    require(accessible.handle() && !accessible.custom() && GetMenuItemInfoW(accessible.handle(), 0, TRUE, &native)
        && !(native.fType & MFT_OWNERDRAW), "Screen-reader and high-contrast path uses native text menu");
    require(TrayMenu::mnemonic_index(L'A') == 0 && TrayMenu::mnemonic_index(L'p') == 1
        && TrayMenu::mnemonic_index(L's') == 3 && TrayMenu::mnemonic_index(L'x') == -1, "Menu mnemonics skip separator and unknown commands");
    require(TrayMenu::mnemonic_index(L'O', i18n::Language::english) == 0
        && TrayMenu::mnemonic_index(L'p', i18n::Language::english) == 1
        && TrayMenu::mnemonic_index(L'x', i18n::Language::english) == 3
        && TrayMenu::mnemonic_index(L's', i18n::Language::english) == -1, "English menu has matching mnemonics");
    for (std::size_t i = 0; i < std::size(i18n::strings); ++i) {
        require(!i18n::strings[i].es.empty() && !i18n::strings[i].en.empty(), "Every catalog entry has both languages");
        for (std::size_t j = i + 1; j < std::size(i18n::strings); ++j)
            require(i18n::strings[i].es != i18n::strings[j].es, "Translation keys are unique");
    }
    require(i18n::translate("RGB 12, 63, 240 · Ajuste 25% · Brillo 2/5 · Velocidad 5/5", i18n::Language::english)
        == "RGB 12, 63, 240 · Adjustment 25% · Brightness 2/5 · Speed 5/5", "Translate compound lighting values without changing numbers");
    require(i18n::translate("Lectura hace 0 s · actualizando", i18n::Language::english) == "Updated 0 s ago · updating"
        && i18n::translate("1 perfil guardado", i18n::Language::english) == "1 saved profile"
        && i18n::translate("24 perfiles guardados", i18n::Language::english) == "24 saved profiles", "Localize fresh readings and singular/plural counts");
    require(!i18n::covered("Texto sin traducir") && !i18n::covered("Brillo x/5")
        && i18n::covered("#8F330A") && i18n::covered("Español"), "Coverage distinguishes untranslated text from invariant values");
    const GeneralPreferences defaults;
    require(defaults.closeToTray && !defaults.minimizeToTray && !defaults.startInTray && defaults.rememberWindow
        && defaults.batteryInterval == 60 && !defaults.bounds && !defaults.mediaClicks, "First run stays visible, with optional startup and media clicks disabled");
    const auto parse = [](const std::string& text) { std::istringstream input(text); return read_general_preferences(input); };
    auto valid = parse("AULA_PREFERENCES 1\r\n1 0 1 1 30\r\n1 -1600 20 -100 920 1\r\n");
    require(valid && valid->language == i18n::Language::spanish && !valid->mediaClicks, "Version 1 keeps defaults without enabling a global hook");
    auto english = parse("AULA_PREFERENCES 2\n1 0 1 1 30 en\n1 -1600 20 -100 920 1\n");
    require(english && english->language == i18n::Language::english && !english->mediaClicks, "Version 2 retains language and leaves gestures opt-in");
    auto gestures = parse("AULA_PREFERENCES 3\n1 0 1 1 30 en 1\n1 -1600 20 -100 920 1\n");
    require(gestures && gestures->restoreLighting, "Migrate existing preferences to the requested automatic restore policy");
    const auto noRestore = parse("AULA_PREFERENCES 4\n1 0 1 1 30 en 1 0\n1 -1600 20 -100 920 1\n");
    require(noRestore && !noRestore->restoreLighting && noRestore->mediaClicks, "Version 4 preserves independent restore and knob preferences");
    require(!parse("AULA_PREFERENCES 4\n1 0 1 1 30 en 1 2\n1 -1600 20 -100 920 1\n")
        && !parse("AULA_PREFERENCES 4\n1 0 1 1 30 en 1\n1 -1600 20 -100 920 1\n"), "Reject incomplete/invalid restore preference");
    require(gestures && gestures->mediaClicks && gestures->startInTray && gestures->batteryInterval == 30
        && gestures->language == i18n::Language::english && gestures->bounds->left == -1600, "Version 3 reads gestures without losing existing settings");
    require(!parse("AULA_PREFERENCES 3\n1 0 1 1 30 en 2\n0 0 0 1440 900 0\n")
        && !parse("AULA_PREFERENCES 3\n1 0 1 1 30 en\n0 0 0 1440 900 0\n"), "Reject malformed gesture settings");
    require(!parse("AULA_PREFERENCES 2\n1 0 1 1 30 fr\n0 0 0 1440 900 0\n"), "Reject unsupported language without rewriting settings");
    require(valid && valid->startInTray && valid->batteryInterval == 30 && valid->bounds
        && valid->bounds->maximized && valid->bounds->left == -1600, "Read negative monitor coordinates and maximized placement");
    for (const auto& text : {"AULA_PREFERENCES 2\n1 0 0 1 60\n0 0 0 1440 900 0\n",
        "AULA_PREFERENCES 1\n2 0 0 1 60\n0 0 0 1440 900 0\n",
        "AULA_PREFERENCES 1\n1 0 0 1 1\n0 0 0 1440 900 0\n",
        "AULA_PREFERENCES 1\n1 0 0 1 60\n1 0 0 1 1 0\n",
        "AULA_PREFERENCES 1\n1 0 0 1 60\n1 0 0 1440 900 0\ntrailing",
        "AULA_PREFERENCES 1\n1 0 0 1 60\n1 -2147483648 0 2147483647 900 0\n"})
        require(!parse(text), "Reject incomplete, future, invalid and overflowing preferences");
    const auto directory = std::filesystem::temp_directory_path() / (L"AulaControl-preferences-test-" + std::to_wstring(GetCurrentProcessId()));
    const auto path = directory / L"preferences.txt";
    GeneralPreferenceStore store(path);
    require(!store.load().bounds && store.readable(), "Missing preference file is a normal first run");
    require(store.save(*gestures), "Persist preferences atomically");
    auto loaded = store.load();
    require(loaded.startInTray && loaded.batteryInterval == 30 && loaded.bounds && loaded.bounds->left == -1600
        && loaded.bounds->maximized && loaded.language == i18n::Language::english && loaded.mediaClicks, "Restore settings, language and enabled gestures after reopening store");
    loaded.mediaClicks = false; loaded.restoreLighting = false;
    require(store.save(loaded) && !GeneralPreferenceStore(path).load().mediaClicks
        && !GeneralPreferenceStore(path).load().restoreLighting, "Disabling gestures and automatic lighting is also remembered");
    {
        std::ofstream damaged(path, std::ios::binary | std::ios::trunc); damaged << "AULA_PREFERENCES 9\nfuture settings\n";
    }
    GeneralPreferenceStore future(path); future.load();
    require(!future.readable() && !future.save(defaults), "Do not overwrite an unknown preference format");
    std::ifstream preserved(path); std::string header; std::getline(preserved, header); preserved.close();
    require(header == "AULA_PREFERENCES 9", "Future settings survive attempted saving");
    std::filesystem::remove(path); std::filesystem::remove(directory);

    const auto instanceName = L"Local\\AulaControl.Test." + std::to_wstring(GetCurrentProcessId());
    {
        SingleInstance owner, second, background;
        require(owner.acquire(false, instanceName) == SingleInstance::Result::owner, "Acquire session once");
        require(second.acquire(true, instanceName) == SingleInstance::Result::existing, "Second normal launch reuses first instance");
        require(owner.restore_requested() && !owner.restore_requested(), "Restore event delivered once, including before UI exists");
        require(background.acquire(false, instanceName) == SingleInstance::Result::existing && !owner.restore_requested(),
            "An automatic duplicate does not interrupt the existing window");
    }
    { SingleInstance replacement; require(replacement.acquire(false, instanceName) == SingleInstance::Result::owner, "Exit releases the single-instance guard"); }
    const auto executable = L"C:\\Mis programas\\Teclado ñ\\aula_control.exe";
    require(startup_command(executable) == L"\"C:\\Mis programas\\Teclado ñ\\aula_control.exe\" --background", "Startup quotes a Unicode executable path");
    require(startup_command(L"C:\\bad\".exe").empty() && startup_command(std::wstring(260, L'x')).empty(), "Reject malformed and oversized Run commands");
    // Only a process-specific test key is touched. Never register the test at logon.
    const auto key = L"Software\\AulaControl-Integration-Test-" + std::to_wstring(GetCurrentProcessId());
    StartupRegistration registration(executable, key);
    require(registration.enabled() == false, "Absent startup entry is disabled");
    require(registration.set(true) && registration.enabled() == true, "Round trip startup in isolated registry key");
    StartupRegistration moved(L"C:\\Elsewhere\\aula_control.exe", key);
    require(moved.enabled() == false, "Moved executable does not claim old startup registration");
    require(registration.set(false) && registration.enabled() == false && registration.set(false), "Disable removes only the application value and is idempotent");
    require(RegDeleteKeyW(HKEY_CURRENT_USER, key.c_str()) == ERROR_SUCCESS, "Remove isolated test key");
    require(!capture_window_bounds(nullptr) && !restore_window_bounds(nullptr, {}), "Missing native window cannot produce a saved placement");
    return 0;
}
