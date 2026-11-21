#pragma once
#include <raven.hpp>
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace ui {
enum class Skin { glass, neumorphic };
enum class Atmosphere { aurora, keyboard };

struct Appearance {
    Skin skin = Skin::glass;
    Atmosphere atmosphere = Atmosphere::aurora;
};

inline Appearance read_appearance(std::istream& input) {
    std::string version, skin, atmosphere;
    if (!std::getline(input, version) || version != "AULA_APPEARANCE 1"
        || !(input >> skin >> atmosphere)) return {};
    input >> std::ws;
    if (!input.eof() || (skin != "glass" && skin != "neumorphic")
        || (atmosphere != "aurora" && atmosphere != "keyboard")) return {};
    return {skin == "glass" ? Skin::glass : Skin::neumorphic,
        atmosphere == "aurora" ? Atmosphere::aurora : Atmosphere::keyboard};
}
inline Appearance load_appearance(const std::filesystem::path& path) {
    std::error_code ec;
    if (path.empty() || std::filesystem::file_size(path, ec) > 128 || ec) return {};
    std::ifstream input(path, std::ios::binary);
    return read_appearance(input);
}
inline bool save_appearance(const std::filesystem::path& path, Appearance value) {
    if (path.empty()) return false;
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) return false;
    auto temporary = path; temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId());
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output << "AULA_APPEARANCE 1\n" << (value.skin == Skin::glass ? "glass" : "neumorphic")
            << ' ' << (value.atmosphere == Atmosphere::aurora ? "aurora" : "keyboard") << '\n';
        output.close();
        if (!output) { std::filesystem::remove(temporary, ec); return false; }
    }
    if (MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
    std::filesystem::remove(temporary, ec); return false;
}

inline void apply_theme(Skin skin) {
    const bool glass = skin == Skin::glass;
    raven::look_set(glass ? R_LOOK_RAVEN_GLASS_DARK : R_LOOK_RAVEN_DARK);
    raven::theme_set_accent(.18f, .77f, .88f);
    raven::theme_set_color(R_COLOR_ACCENT_HOVERED, .29f, .84f, .94f, 1);
    raven::theme_set_color(R_COLOR_ACCENT_PRESSED, .12f, .63f, .74f, 1);
    raven::theme_set_color(R_COLOR_ACCENT_TEXT, .025f, .10f, .14f, 1);
    raven::theme_set_color(R_COLOR_WINDOW, .12f, .135f, .155f, 1);
    raven::theme_set_color(R_COLOR_PANEL, .15f, .168f, .19f, glass ? .34f : 1);
    raven::theme_set_color(R_COLOR_CONTROL, .65f, .77f, .88f, glass ? .12f : 1);
    if (!glass) raven::theme_set_color(R_COLOR_CONTROL, .18f, .20f, .235f, 1);
    raven::theme_set_color(R_COLOR_CONTROL_HOVERED, glass ? .8f : .23f, glass ? .9f : .26f, glass ? 1 : .30f, glass ? .22f : 1);
    raven::theme_set_color(R_COLOR_CONTROL_PRESSED, .11f, .14f, .17f, glass ? .55f : 1);
    raven::theme_set_color(R_COLOR_CONTROL_BORDER, .64f, .77f, .89f, glass ? .17f : .16f);
    raven::theme_set_color(R_COLOR_FOCUS, .39f, .86f, .96f, 1);
    raven::theme_set_color(R_COLOR_INPUT, .08f, .10f, .15f, glass ? .50f : 1);
    raven::theme_set_color(R_COLOR_TRACK, .08f, .095f, .12f, glass ? .60f : 1);
    raven::theme_set_color(R_COLOR_TEXT, .94f, .96f, .99f, 1);
    raven::theme_set_color(R_COLOR_TEXT_DISABLED, .46f, .53f, .62f, 1);
    raven::theme_set_color(R_COLOR_TEXT_PLACEHOLDER, .68f, .74f, .83f, 1);
    raven::theme_set_color(R_COLOR_POPUP, .09f, .12f, .17f, glass ? .88f : 1);
    raven::theme_set_color(R_COLOR_POPUP_BORDER, .39f, .51f, .64f, .35f);
    raven::theme_set_color(R_COLOR_SELECTION, .15f, .40f, .48f, .75f);
    raven::theme_set_color(R_COLOR_ROW_HOVERED, .23f, .40f, .49f, .32f);
    raven::theme_set_color(R_COLOR_TOOLTIP, .07f, .095f, .13f, .98f);
    raven::theme_set_color(R_COLOR_TOOLTIP_TEXT, .86f, .92f, .97f, 1);
    raven::theme_set_metric(R_METRIC_CORNER_RADIUS, 12);
    raven::theme_set_metric(R_METRIC_POPUP_RADIUS, 12);
    raven::theme_set_metric(R_METRIC_BORDER_WIDTH, 1);
    raven::theme_set_metric(R_METRIC_SLIDER_HANDLE_SIZE, 14);
    raven::theme_set_metric(R_METRIC_CONTROL_HEIGHT, 42);
    raven::theme_set_metric(R_METRIC_FONT_SIZE, 14);
    raven::theme_set_metric(R_METRIC_GLASS_FROST, .85f);
    raven::theme_set_metric(R_METRIC_GLASS_REFRACTION, .24f);
    raven::theme_set_metric(R_METRIC_GLASS_SATURATION, .42f);
    raven::theme_set_metric(R_METRIC_GLASS_RIM, .27f);
    raven::theme_set_metric(R_METRIC_GLASS_SHADOW, .65f);
}

// A cached, low-resolution field of diffused light. No frame-by-frame CPU work,
// desktop capture or external image is needed. Glass samples it through Vulkan.
class AmbientBackground {
public:
    void update(raven::Window window, Atmosphere atmosphere, float red, float green, float blue, bool multicolor, Skin skin) {
        const bool neutral = skin == Skin::neumorphic;
        const bool follow = !neutral && atmosphere == Atmosphere::keyboard && !multicolor;
        const std::array<int, 5> key{{neutral ? 1 : 0, follow ? 1 : 0,
            follow ? static_cast<int>(red * 255) : 0,
            follow ? static_cast<int>(green * 255) : 0,
            follow ? static_cast<int>(blue * 255) : 0}};
        if (texture_.is_valid() && key == key_) { window.set_background(texture_); return; }
        constexpr int width = 640, height = 400;
        std::vector<uint8_t> pixels(width * height * 4);
        const std::array<float, 3> first = neutral ? std::array<float, 3>{{.025f, .03f, .038f}}
            : follow ? std::array<float, 3>{{red * .26f, green * .26f, blue * .26f}}
            : std::array<float, 3>{{.02f, .23f, .27f}};
        for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
            const float u = x / float(width - 1), v = y / float(height - 1);
            auto field = [&](float cx, float cy, float sx, float sy) {
                const float dx = (u - cx) / sx, dy = (v - cy) / sy;
                return std::exp(-2.0f * (dx * dx + dy * dy));
            };
            const float a = field(.17f, .18f, .65f, .90f);
            const float b = field(.90f, .72f, .60f, .90f);
            const float c = field(.57f, -.20f, .75f, .65f);
            const std::array<float, 3> base = neutral ? std::array<float, 3>{{.09f, .105f, .13f}} : std::array<float, 3>{{.04f, .055f, .085f}};
            const std::array<float, 3> plum = neutral ? std::array<float, 3>{{.012f, .018f, .026f}} : std::array<float, 3>{{.16f, .08f, .23f}};
            const std::array<float, 3> haze = neutral ? std::array<float, 3>{{.018f, .022f, .028f}} : std::array<float, 3>{{.10f, .14f, .18f}};
            const std::size_t at = (static_cast<std::size_t>(y) * width + x) * 4;
            for (int channel = 0; channel < 3; ++channel) {
                // Deterministic sub-byte dither prevents banding in the broad gradients.
                const float dither = (int((x * 13 + y * 29) % 17) - 8) / 255.0f / 17.0f;
                pixels[at + channel] = static_cast<uint8_t>(std::clamp(base[channel] + first[channel] * a
                    + plum[channel] * b + haze[channel] * c + dither, 0.0f, 1.0f) * 255);
            }
            pixels[at + 3] = 255;
        }
        if (texture_.is_valid()) texture_.update(pixels.data(), width, height);
        else texture_ = raven::Texture::create_from_pixels(pixels.data(), width, height);
        key_ = key; window.set_background(texture_);
    }
private:
    raven::Texture texture_;
    std::array<int, 5> key_{{-1, -1, -1, -1, -1}};
};
} // namespace ui
