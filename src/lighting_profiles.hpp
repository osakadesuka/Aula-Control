#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

struct LightingEffect {
    const char* name;
    int mode;
    bool multicolorVerified = false;
    bool builtInPalette = false;
    bool animated = true;
};
inline constexpr std::array<LightingEffect, 18> kLightingEffects{{
    {"Color fijo", 1, true, false, false}, {"Reactivo al pulsar", 2, true}, {"Apagado aleatorio", 4, true}, {"Lluvia", 5, true},
    {"Confeti estático", 6, true, true, false}, {"Respiración", 7, true}, {"Transición gradual de colores", 8, true, true},
    {"Ola desde el centro", 9, true}, {"Ola vertical", 10, true}, {"Ola horizontal", 11, true}, {"Vórtice", 12, true},
    {"Reactivo por fila", 13, true}, {"Barrido horizontal reactivo", 14, true}, {"Ola reactiva", 15, true},
    {"Cascada por filas", 16, true}, {"Látigo desde el centro", 17, true}, {"Ola diagonal rápida", 18, true},
    {"Filas en direcciones opuestas", 19, true}
}};
inline const LightingEffect* lighting_effect(int mode) {
    for (const auto& effect : kLightingEffects) if (effect.mode == mode) return &effect;
    return nullptr;
}
inline bool lighting_has_speed(int mode) { const auto* effect = lighting_effect(mode); return effect && effect->animated; }
inline bool lighting_uses_builtin_palette(int mode) { const auto* effect = lighting_effect(mode); return effect && effect->builtInPalette; }
inline bool lighting_multicolor_verified(int mode) { const auto* effect = lighting_effect(mode); return effect && effect->multicolorVerified; }

struct LightingSettings {
    int red = 143, green = 51, blue = 10, mode = 1, intensity = 100;
    bool multicolor = false;
    int speed = 3;
    int brightness = 3;
    bool valid() const {
        return red >= 0 && red <= 255 && green >= 0 && green <= 255 && blue >= 0 && blue <= 255
            && intensity >= 0 && intensity <= 100 && speed >= 1 && speed <= 5 && brightness >= 1 && brightness <= 5
            && std::any_of(kLightingEffects.begin(), kLightingEffects.end(), [&](const auto& e) { return e.mode == mode; });
    }
    bool operator==(const LightingSettings& other) const {
        return red == other.red && green == other.green && blue == other.blue && mode == other.mode
            && intensity == other.intensity && multicolor == other.multicolor && speed == other.speed && brightness == other.brightness;
    }
};
struct LightingProfile { std::string name; LightingSettings settings; };

inline std::wstring lighting_tool_arguments(const LightingSettings& settings) {
    if (!settings.valid()) return {};
    const bool ownPalette = settings.multicolor || lighting_uses_builtin_palette(settings.mode);
    const float intensity = ownPalette ? 1.0f : settings.intensity / 100.0f;
    const auto channel = [&](int value) { return std::to_wstring(static_cast<int>(std::lround(value * intensity))); };
    return channel(settings.red) + L" " + channel(settings.green) + L" " + channel(settings.blue)
        + L" --modo " + std::to_wstring(settings.mode)
        + L" --velocidad " + std::to_wstring(lighting_has_speed(settings.mode) ? settings.speed : 3)
        + L" --brillo " + std::to_wstring(settings.brightness)
        + (settings.multicolor ? L" --multicolor" : L"") + L" --aplicar";
}

inline std::string profile_name(std::string name) {
    const auto first = name.find_first_not_of(' ');
    if (first == std::string::npos) return {};
    return name.substr(first, name.find_last_not_of(' ') - first + 1);
}
inline bool valid_profile(const LightingProfile& profile) {
    return !profile.name.empty() && profile.name.size() <= 96 && profile_name(profile.name) == profile.name
        && std::none_of(profile.name.begin(), profile.name.end(), [](unsigned char c) { return c < 32 || c == 127; })
        && profile.settings.valid();
}

// A complete read is required before accepting data; malformed files are never overwritten.
inline bool read_profiles(std::istream& input, std::vector<LightingProfile>& profiles) {
    std::string line;
    if (!std::getline(input, line)) return false;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const bool version2 = line == "AULA_LIGHTING_PROFILES 2";
    const bool version3 = line == "AULA_LIGHTING_PROFILES 3";
    if (!version2 && !version3 && line != "AULA_LIGHTING_PROFILES 1") return false;
    std::vector<LightingProfile> parsed;
    while (std::getline(input, line)) {
        if (line.find_first_not_of(" \t\r") == std::string::npos) continue;
        std::istringstream row(line);
        LightingProfile p; int multicolor = 0;
        auto& s = p.settings;
        if (!(row >> std::quoted(p.name) >> s.red >> s.green >> s.blue >> s.mode >> s.intensity >> multicolor)) return false;
        if ((version2 || version3) && !(row >> s.speed)) return false;
        if (version3 && !(row >> s.brightness)) return false;
        s.multicolor = multicolor == 1;
        row >> std::ws;
        if (!row.eof() || multicolor < 0 || multicolor > 1 || !valid_profile(p) || parsed.size() >= 24
            || std::any_of(parsed.begin(), parsed.end(), [&](const auto& existing) { return existing.name == p.name; })) return false;
        parsed.push_back(std::move(p));
    }
    if (input.bad()) return false;
    profiles = std::move(parsed); return true;
}

enum class ProfileResult { saved, updated, invalid, full, io_error, unreadable };
class ProfileStore {
public:
    explicit ProfileStore(std::filesystem::path path) : path_(std::move(path)) {}
    bool load() {
        if (path_.empty()) return readable_ = false;
        std::error_code ec;
        const bool exists = std::filesystem::exists(path_, ec);
        if (ec) return readable_ = false;
        if (!exists) { profiles_.clear(); return readable_ = true; }
        const auto size = std::filesystem::file_size(path_, ec);
        if (ec || size > 65536) return readable_ = false;
        std::ifstream file(path_, std::ios::binary);
        return readable_ = file && read_profiles(file, profiles_);
    }
    const std::vector<LightingProfile>& entries() const { return profiles_; }
    bool readable() const { return readable_; }
    ProfileResult save(LightingProfile p) {
        if (!readable_) return ProfileResult::unreadable;
        p.name = profile_name(std::move(p.name));
        if (!valid_profile(p)) return ProfileResult::invalid;
        auto next = profiles_;
        const auto at = std::find_if(next.begin(), next.end(), [&](const auto& entry) { return entry.name == p.name; });
        const bool update = at != next.end();
        if (update) *at = std::move(p);
        else { if (next.size() >= 24) return ProfileResult::full; next.push_back(std::move(p)); }
        if (!commit(next)) return ProfileResult::io_error;
        return update ? ProfileResult::updated : ProfileResult::saved;
    }
    bool erase(std::size_t index) {
        if (!readable_ || index >= profiles_.size()) return false;
        auto next = profiles_; next.erase(next.begin() + index);
        return commit(next);
    }
private:
    bool commit(const std::vector<LightingProfile>& next) {
        std::error_code ec;
        std::filesystem::create_directories(path_.parent_path(), ec);
        if (ec) return false;
        auto temporary = path_;
        temporary += L"." + std::to_wstring(GetCurrentProcessId()) + L".tmp";
        {
            std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
            out << "AULA_LIGHTING_PROFILES 3\n";
            for (const auto& p : next) {
                const auto& s = p.settings;
                out << std::quoted(p.name) << ' ' << s.red << ' ' << s.green << ' ' << s.blue << ' '
                    << s.mode << ' ' << s.intensity << ' ' << (s.multicolor ? 1 : 0) << ' ' << s.speed << ' ' << s.brightness << '\n';
            }
            out.close();
            if (!out) { std::filesystem::remove(temporary, ec); return false; }
        }
        if (!MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            std::filesystem::remove(temporary, ec); return false;
        }
        profiles_ = next; return true;
    }
    std::filesystem::path path_;
    std::vector<LightingProfile> profiles_;
    bool readable_ = true;
};
