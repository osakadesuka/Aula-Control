#pragma once
#include "lighting_profiles.hpp"
#include "control_channel.hpp"
#include <charconv>
#include <chrono>
#include <optional>
#include <cstdint>

inline std::int64_t lighting_wall_time_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
struct LightingReceipt { ControlTransport transport; std::int64_t timeMs; };
inline std::optional<LightingReceipt> lighting_receipt(std::string_view output) {
    std::optional<LightingReceipt> result;
    constexpr std::string_view family = "AULA_LIGHTING ", prefix = "AULA_LIGHTING 1 ";
    while (!output.empty()) {
        const auto end = output.find('\n'); auto line = output.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.substr(0, family.size()) == family) {
            if (result || line.substr(0, prefix.size()) != prefix) return {};
            line.remove_prefix(prefix.size()); const auto space = line.find(' ');
            if (space == std::string_view::npos) return {};
            const auto transport = parse_transport(line.substr(0, space)); line.remove_prefix(space + 1);
            std::int64_t time = 0;
            const auto parsed = std::from_chars(line.data(), line.data() + line.size(), time);
            if (!transport || parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size() || time <= 0) return {};
            result = LightingReceipt{*transport, time};
        }
        if (end == std::string_view::npos) break;
        output.remove_prefix(end + 1);
    }
    return result;
}
struct LastLighting {
    LightingSettings settings;
    LightingReceipt receipt;
};
inline std::optional<LastLighting> read_last_lighting(std::istream& input) {
    std::string header, transport; int multicolor = 0;
    LastLighting result{}; auto& s = result.settings;
    if (!std::getline(input, header)) return {};
    if (!header.empty() && header.back() == '\r') header.pop_back();
    if (header != "AULA_LAST_LIGHTING 1" || !(input >> s.red >> s.green >> s.blue >> s.mode >> s.intensity
            >> multicolor >> s.speed >> s.brightness >> transport >> result.receipt.timeMs)) return {};
    s.multicolor = multicolor == 1;
    const auto parsed = parse_transport(transport);
    input >> std::ws;
    if (input.bad() || !input.eof() || (multicolor != 0 && multicolor != 1) || !s.valid() || !parsed || result.receipt.timeMs <= 0) return {};
    result.receipt.transport = *parsed;
    return result;
}
class LastLightingStore {
public:
    explicit LastLightingStore(std::filesystem::path path) : path_(std::move(path)) {}
    std::optional<LastLighting> load() {
        if (path_.empty()) { readable_ = false; return {}; }
        std::error_code ec;
        const bool exists = std::filesystem::exists(path_, ec);
        if (!ec && !exists) return {};
        if (ec || std::filesystem::file_size(path_, ec) > 1024 || ec) { readable_ = false; return {}; }
        std::ifstream input(path_, std::ios::binary);
        auto value = read_last_lighting(input); readable_ = value.has_value(); return value;
    }
    bool readable() const { return readable_; }
    bool save(const LastLighting& value) const {
        if (!readable_ || path_.empty() || !value.settings.valid() || value.receipt.timeMs <= 0) return false;
        std::error_code ec; std::filesystem::create_directories(path_.parent_path(), ec);
        if (ec) return false;
        auto temporary = path_; temporary += L".tmp-" + std::to_wstring(GetCurrentProcessId());
        const auto& s = value.settings;
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output << "AULA_LAST_LIGHTING 1\n" << s.red << ' ' << s.green << ' ' << s.blue << ' ' << s.mode << ' '
                << s.intensity << ' ' << s.multicolor << ' ' << s.speed << ' ' << s.brightness << '\n'
                << transport_name(value.receipt.transport) << ' ' << value.receipt.timeMs << '\n';
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

// A local simulation clock, not firmware LED telemetry. Both views can sample
// the same applied timeline without double-counting time or resetting on edits.
class LightingTimeline {
public:
    using Clock = std::chrono::steady_clock;
    explicit LightingTimeline(Clock::time_point now = Clock::now()) : tick_(now) {}
    double sample(Clock::time_point now, bool animate) {
        if (animate && now > tick_) phase_ += std::chrono::duration<double>(now - tick_).count() * speed_ / 3.0;
        tick_ = now; return phase_;
    }
    void set_speed(int speed, Clock::time_point now, bool animate) {
        sample(now, animate); speed_ = std::clamp(speed, 1, 5);
    }
    void restart(int speed, double elapsedSeconds, Clock::time_point now) {
        speed_ = std::clamp(speed, 1, 5); phase_ = (std::max)(0.0, elapsedSeconds) * speed_ / 3.0; tick_ = now;
    }
private:
    Clock::time_point tick_;
    double phase_ = 0;
    int speed_ = 3;
};
inline double lighting_elapsed_seconds(std::int64_t sent, std::int64_t now) {
    if (sent <= 0 || now <= sent) return 0;
    return (static_cast<double>(now) - static_cast<double>(sent)) / 1000.0;
}

class LightingRestorePolicy {
public:
    bool due(bool enabled, bool saved, bool ready, unsigned connection) const {
        return enabled && saved && ready && (!attempted_ || *attempted_ != connection);
    }
    void claim(unsigned connection) { attempted_ = connection; }
private:
    std::optional<unsigned> attempted_;
};
