#pragma once
#include <charconv>
#include <chrono>
#include <optional>
#include <string_view>

inline std::optional<int> battery_percent(std::string_view output) {
    constexpr std::string_view marker = "Porcentaje informado (experimental): ";
    std::optional<int> result;
    while (!output.empty()) {
        const auto end = output.find('\n');
        auto line = output.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.substr(0, marker.size()) == marker) {
            if (result) return {}; // Ambiguous output must not replace a valid previous reading.
            line.remove_prefix(marker.size());
            if (line.size() < 2 || line.back() != '%') return {};
            line.remove_suffix(1);
            int value = -1;
            const auto parsed = std::from_chars(line.data(), line.data() + line.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != line.data() + line.size() || value < 0 || value > 100) return {};
            result = value;
        }
        if (end == std::string_view::npos) break;
        output.remove_prefix(end + 1);
    }
    return result;
}

class BatteryState {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    enum class Phase { waiting, updating, current, stale, unavailable };
    explicit BatteryState(Time now) : next_(now) {}
    bool due(Time now) const { return !updating_ && now >= next_; }
    void start() { updating_ = true; }
    void invalidate() { failed_ = true; }
    void request_now(Time now) { if (!updating_) next_ = now; }
    void finish(std::optional<int> value, Time now) {
        updating_ = false; attempted_ = now; failed_ = !value || *value < 0 || *value > 100;
        if (!failed_) { percent_ = value; received_ = now; }
        next_ = now + std::chrono::seconds(interval_);
    }
    void set_interval(int seconds, Time now) {
        if (seconds != 30 && seconds != 60) return;
        interval_ = seconds;
        next_ = attempted_ ? *attempted_ + std::chrono::seconds(interval_) : now;
    }
    int interval() const { return interval_; }
    std::optional<int> percent() const { return percent_; }
    std::optional<long long> age(Time now) const {
        if (!received_) return {};
        return std::chrono::duration_cast<std::chrono::seconds>(now - *received_).count();
    }
    long long remaining(Time now) const {
        if (now >= next_) return 0;
        return std::chrono::duration_cast<std::chrono::seconds>(next_ - now + std::chrono::milliseconds(999)).count();
    }
    Phase phase(Time now) const {
        if (updating_) return Phase::updating;
        if (!percent_) return failed_ ? Phase::unavailable : Phase::waiting;
        return failed_ || now >= next_ + std::chrono::seconds(5) ? Phase::stale : Phase::current;
    }
private:
    std::optional<int> percent_;
    std::optional<Time> received_, attempted_;
    Time next_;
    int interval_ = 60;
    bool updating_ = false, failed_ = false;
};
