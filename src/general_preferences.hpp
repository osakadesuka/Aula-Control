#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include "localization.hpp"

struct WindowBounds {
    int left = 0, top = 0, right = 1440, bottom = 900;
    bool maximized = false;
    bool valid() const {
        const auto width = static_cast<long long>(right) - left, height = static_cast<long long>(bottom) - top;
        return left >= -100000 && top >= -100000 && right <= 100000 && bottom <= 100000
            && width >= 200 && width <= 16384 && height >= 200 && height <= 16384;
    }
};
struct GeneralPreferences {
    bool closeToTray = true, minimizeToTray = false, startInTray = false, rememberWindow = true;
    int batteryInterval = 60;
    std::optional<WindowBounds> bounds;
    i18n::Language language = i18n::Language::spanish;
    bool mediaClicks = false;
    bool restoreLighting = true;
};
inline std::optional<GeneralPreferences> read_general_preferences(std::istream& input) {
    std::string header;
    int close, minimize, background, remember, interval, positioned, maximized;
    WindowBounds bounds;
    if (!std::getline(input, header)) return {};
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (header != "AULA_PREFERENCES 1" && header != "AULA_PREFERENCES 2" && header != "AULA_PREFERENCES 3" && header != "AULA_PREFERENCES 4") return {};
    std::string language = "es";
    int mediaClicks = 0, restoreLighting = 1;
    if (!(input >> close >> minimize >> background >> remember >> interval)) return {};
    if (header != "AULA_PREFERENCES 1" && (!(input >> language) || (language != "es" && language != "en"))) return {};
    if ((header == "AULA_PREFERENCES 3" || header == "AULA_PREFERENCES 4") && (!(input >> mediaClicks) || (mediaClicks != 0 && mediaClicks != 1))) return {};
    if (header == "AULA_PREFERENCES 4" && (!(input >> restoreLighting) || (restoreLighting != 0 && restoreLighting != 1))) return {};
    if (!(input >> positioned
            >> bounds.left >> bounds.top >> bounds.right >> bounds.bottom >> maximized)) return {};
    for (const int value : {close, minimize, background, remember, positioned, maximized})
        if (value != 0 && value != 1) return {};
    input >> std::ws;
    if (!input.eof() || (interval != 30 && interval != 60) || !bounds.valid()) return {};
    GeneralPreferences result{close != 0, minimize != 0, background != 0, remember != 0, interval, {}};
    result.language = language == "en" ? i18n::Language::english : i18n::Language::spanish;
    result.mediaClicks = mediaClicks != 0;
    result.restoreLighting = restoreLighting != 0;
    bounds.maximized = maximized != 0;
    if (positioned) result.bounds = bounds;
    return result;
}
class GeneralPreferenceStore {
public:
    explicit GeneralPreferenceStore(std::filesystem::path path) : path_(std::move(path)) {}
    GeneralPreferences load() {
        std::error_code ec;
        if (path_.empty()) { readable_ = false; return {}; }
        const bool exists = std::filesystem::exists(path_, ec);
        if (!ec && !exists) return {};
        if (ec || std::filesystem::file_size(path_, ec) > 2048 || ec) { readable_ = false; return {}; }
        std::ifstream input(path_, std::ios::binary);
        auto parsed = read_general_preferences(input);
        readable_ = parsed.has_value();
        return parsed.value_or(GeneralPreferences{});
    }
    bool readable() const { return readable_; }
    bool save(const GeneralPreferences& value) const {
        if (!readable_ || path_.empty() || (value.batteryInterval != 30 && value.batteryInterval != 60)
            || (value.bounds && !value.bounds->valid())) return false;
        std::error_code ec;
        std::filesystem::create_directories(path_.parent_path(), ec);
        if (ec) return false;
        auto temporary = path_; temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId());
        const auto bounds = value.bounds.value_or(WindowBounds{});
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output << "AULA_PREFERENCES 4\n" << value.closeToTray << ' ' << value.minimizeToTray << ' '
                << value.startInTray << ' ' << value.rememberWindow << ' ' << value.batteryInterval << ' ' << i18n::code(value.language) << ' ' << value.mediaClicks << ' ' << value.restoreLighting << '\n'
                << value.bounds.has_value() << ' ' << bounds.left << ' ' << bounds.top << ' '
                << bounds.right << ' ' << bounds.bottom << ' ' << bounds.maximized << '\n';
            output.close();
            if (!output) { std::filesystem::remove(temporary, ec); return false; }
        }
        if (MoveFileExW(temporary.c_str(), path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return true;
        std::filesystem::remove(temporary, ec); return false;
    }
private:
    std::filesystem::path path_;
    bool readable_ = true;
};
