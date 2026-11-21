#include "battery_state.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

void require(bool ok, const char* reason) {
    if (!ok) { std::cerr << reason << '\n'; std::exit(1); }
}
int main() {
    constexpr auto prefix = "Porcentaje informado (experimental): ";
    require(battery_percent(std::string(prefix) + "0%\r\n") == 0, "Zero is a valid reading");
    require(battery_percent(std::string("Diagnostic\n") + prefix + "100%\n") == 100, "Valid complete line");
    for (const auto* bad : {"-1%", "101%", "95junk%", "95", "95% garbage", "9999999999999999999999%", "%"})
        require(!battery_percent(std::string(prefix) + bad), "Reject malformed percentage");
    require(!battery_percent(std::string(prefix) + "0%\n" + prefix + "95%\n"), "Reject ambiguous output");
    require(!battery_percent(std::string("unrelated ") + prefix + "95%\n"), "Reject marker inside unrelated line");

    using namespace std::chrono_literals;
    using Phase = BatteryState::Phase;
    const auto t = BatteryState::Time{};
    BatteryState state(t);
    require(state.due(t) && state.phase(t) == Phase::waiting, "Immediate first query");
    state.start(); require(!state.due(t) && state.phase(t) == Phase::updating, "No overlapping queries");
    state.finish(95, t);
    require(state.percent() == 95 && state.age(t + 4s) == 4 && state.remaining(t) == 60, "Track reading and next query");
    require(!state.due(t + 59s) && state.due(t + 60s), "Default interval boundary");
    state.start(); state.finish({}, t + 60s);
    require(state.percent() == 95 && state.age(t + 65s) == 65 && state.phase(t + 65s) == Phase::stale, "Failure preserves last valid reading and age");
    state.set_interval(30, t + 65s);
    require(state.remaining(t + 65s) == 25 && state.due(t + 90s), "Interval changes from last attempt");
    state.set_interval(1, t + 65s); require(state.interval() == 30, "Reject unsupported interval");
    state.start(); state.finish(0, t + 91s);
    require(state.percent() == 0 && state.age(t + 91s) == 0 && state.phase(t + 91s) == Phase::current, "Recover from failure with empty battery");
    state.finish(101, t + 92s); require(state.percent() == 0, "Invalid result never replaces last value");
    BatteryState missing(t); missing.start(); missing.finish({}, t);
    require(!missing.percent() && missing.phase(t) == Phase::unavailable, "Missing data is not zero percent");
    require(state.phase(t + 200s) == Phase::stale, "Delayed queries become stale");
    BatteryState reconnect(t); reconnect.finish(95, t); reconnect.invalidate(); reconnect.request_now(t + 10s);
    require(reconnect.due(t + 10s) && reconnect.percent() == 95 && reconnect.age(t + 10s) == 10, "Reconnect requests immediate refresh without erasing the old reading");
    reconnect.start(); reconnect.request_now(t + 11s);
    require(!reconnect.due(t + 11s), "Reconnect cannot overlap an active query");
    std::cout << "Battery parsing, scheduling, stale retention and recovery OK\n";
}
