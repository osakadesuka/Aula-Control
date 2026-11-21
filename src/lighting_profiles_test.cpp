#include "lighting_profiles.hpp"
#include <cstdlib>
#include <iostream>

void require(bool ok, const char* reason) {
    if (!ok) { std::cerr << reason << '\n'; std::exit(1); }
}
int main() {
    std::vector<LightingProfile> data;
    const auto parse = [&](const std::string& text) { std::istringstream stream(text); return read_profiles(stream, data); };
    require(parse("AULA_LIGHTING_PROFILES 1\r\n\"Rojo\" 255 0 0 1 25 0\r\n"), "CRLF read");
    const auto original = data.front();
    require(original.settings.speed == 3, "Legacy profiles keep the former speed");
    require(original.settings.brightness == 3, "Legacy profiles keep the former native brightness");
    for (const auto& row : {"\"Bad\" 256 0 0 1 50 0", "\"Bad\" 0 0 0 20 50 0", "\"Bad\" 0 0 0 1 -1 0",
                            "\"Bad\" 0 0 0 1 50 2", "\"Bad\" 0 0 0 1", "\"Bad\" 0 0 0 1 50 0 trailing"}) {
        require(!parse(std::string("AULA_LIGHTING_PROFILES 1\n") + row), "Reject invalid or truncated profile");
        require(data.size() == 1 && data[0].name == original.name && data[0].settings == original.settings, "Failed read preserves data");
    }
    require(!parse("AULA_LIGHTING_PROFILES 9\n"), "Reject future format");
    require(lighting_tool_arguments({255, 0, 0, 11, 25, false, 5}) == L"64 0 0 --modo 11 --velocidad 5 --brillo 3 --aplicar", "Scale RGB and forward chosen speed");
    require(lighting_tool_arguments({255, 0, 0, 11, 25, true, 1, 2}) == L"255 0 0 --modo 11 --velocidad 1 --brillo 2 --multicolor --aplicar", "Multicolor uses native brightness without RGB attenuation");
    require(lighting_tool_arguments({23, 154, 221, 8, 25, false, 4, 5}) == L"23 154 221 --modo 8 --velocidad 4 --brillo 5 --aplicar", "Built-in palette forwards native brightness");
    require(lighting_tool_arguments({255, 0, 0, 1, 100, false, 5, 4}) == L"255 0 0 --modo 1 --velocidad 3 --brillo 4 --aplicar", "Static mode retains observed firmware speed and chosen brightness");
    require(lighting_tool_arguments({255, 0, 0, 11, 100, false, 6}).empty(), "Invalid settings produce no command");
    require(lighting_tool_arguments({255, 0, 0, 11, 100, true, 3, 6}).empty(), "Invalid native brightness produces no command");
    require(!parse("AULA_LIGHTING_PROFILES 1\n\"Dup\" 0 0 0 1 0 0\n\"Dup\" 1 1 1 2 100 1\n"), "Reject duplicate names");
    require(parse("AULA_LIGHTING_PROFILES 2\r\n\"Ola\" 255 0 0 11 100 1 5\r\n")
        && data[0].settings.speed == 5 && data[0].settings.multicolor && data[0].settings.brightness == 3, "Read v2 with former brightness");
    for (const auto& tail : {"0", "6", "3 trailing", ""})
        require(!parse(std::string("AULA_LIGHTING_PROFILES 2\n\"Bad\" 255 0 0 11 100 1 ") + tail), "Reject invalid or missing speed");
    require(parse("AULA_LIGHTING_PROFILES 3\r\n\"Ola\" 255 0 0 11 100 1 5 2\r\n")
        && data[0].settings.brightness == 2, "Read native brightness in v3");
    for (const auto& tail : {"0", "6", "3 trailing", ""})
        require(!parse(std::string("AULA_LIGHTING_PROFILES 3\n\"Bad\" 255 0 0 11 100 1 5 ") + tail), "Reject invalid or missing native brightness");

    const auto dir = std::filesystem::temp_directory_path() / (L"aula-profiles-test-" + std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directory(dir);
    const auto path = dir / L"profiles.txt";
    const auto legacyPath = dir / L"legacy.txt";
    { std::ofstream legacy(legacyPath); legacy << "AULA_LIGHTING_PROFILES 1\n\"AFK\" 23 154 221 8 100 0\n\"Rojo\" 255 0 0 11 50 0\n"; }
    ProfileStore legacy(legacyPath);
    require(legacy.load() && legacy.entries().size() == 2 && legacy.entries()[0].settings.speed == 3, "Load legacy library without migration writes");
    const auto retained = legacy.entries()[0];
    require(legacy.save({"Rojo", {255, 0, 0, 11, 50, true, 5, 2}}) == ProfileResult::updated, "Migrate v1 on explicit save");
    ProfileStore migrated(legacyPath);
    require(migrated.load() && migrated.entries().size() == 2 && migrated.entries()[0].name == retained.name
        && migrated.entries()[0].settings == retained.settings && migrated.entries()[1].settings.speed == 5
        && migrated.entries()[1].settings.brightness == 2, "Migration preserves untouched profiles and new brightness");
    { std::ofstream legacy2(legacyPath); legacy2 << "AULA_LIGHTING_PROFILES 2\n\"AFK\" 23 154 221 8 100 0 4\n\"Rojo\" 255 0 0 11 50 1 5\n"; }
    ProfileStore version2(legacyPath);
    require(version2.load() && version2.entries()[0].settings.speed == 4 && version2.entries()[0].settings.brightness == 3, "Load v2 brightness default");
    const auto retained2 = version2.entries()[0];
    require(version2.save({"Rojo", {255, 0, 0, 11, 50, true, 5, 1}}) == ProfileResult::updated, "Migrate v2 on explicit save");
    ProfileStore migrated2(legacyPath);
    require(migrated2.load() && migrated2.entries()[0].settings == retained2.settings && migrated2.entries()[1].settings.brightness == 1, "V2 migration preserves speed and untouched profile");
    ProfileStore store(path);
    require(store.load() && store.entries().empty(), "Missing file starts empty");
    LightingProfile p{u8"Música \"azul\" \\ noche", {0, 128, 255, 7, 25, false}};
    require(store.save(p) == ProfileResult::saved, "Save Unicode and escaped name");
    p.settings.intensity = 75;
    p.settings.speed = 5; p.settings.multicolor = true; p.settings.brightness = 4;
    require(store.save(p) == ProfileResult::updated && store.entries().size() == 1, "Update same name");
    ProfileStore restored(path);
    require(restored.load() && restored.entries()[0].name == p.name && restored.entries()[0].settings == p.settings, "Persist exact settings");
    require(restored.save({"Invalid", {0, 0, 0, 3, 100, false}}) == ProfileResult::invalid, "Reject unverified mode");
    require(restored.save({"Invalid speed", {0, 0, 0, 11, 100, false, 6}}) == ProfileResult::invalid, "Reject unsupported speed");
    require(restored.save({"Invalid brightness", {0, 0, 0, 11, 100, true, 3, 0}}) == ProfileResult::invalid, "Reject unsupported native brightness");
    for (int i = 1; i < 24; ++i) require(restored.save({"Profile " + std::to_string(i), {}}) == ProfileResult::saved, "Save up to limit");
    require(restored.save({"Overflow", {}}) == ProfileResult::full, "Enforce profile count limit");
    require(restored.erase(0), "Erase selected profile");
    require(restored.save(p) == ProfileResult::saved, "Restore erased profile");

    const auto badPath = dir / L"bad.txt";
    { std::ofstream bad(badPath); bad << "unknown format"; }
    ProfileStore bad(badPath);
    require(!bad.load() && bad.save(p) == ProfileResult::unreadable, "Protect unreadable file from overwrite");
    ProfileStore blocked(badPath / L"profiles.txt");
    require(blocked.save(p) == ProfileResult::io_error && blocked.entries().empty(), "Failed write preserves memory");
    std::filesystem::remove(path); std::filesystem::remove(badPath); std::filesystem::remove(legacyPath); std::filesystem::remove(dir);
    std::cout << "Profile validation, persistence, update, delete/restore and failure protection OK\n";
}
