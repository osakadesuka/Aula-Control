#include "device_state.hpp"
#include "battery_state.hpp"
#include <cstdlib>
#include <iostream>

void require(bool ok, const char* reason) { if (!ok) { std::cerr << reason << '\n'; std::exit(1); } }
int main() {
    require(device_reply("Diagnostic\nAULA_DEVICE 1 ready 0\r\n")->percent == 0, "Zero battery in complete frame");
    require(device_reply("AULA_DEVICE 1 ready 100\n")->percent == 100, "Full battery");
    require(device_reply("AULA_DEVICE 1 absent\n")->code == DeviceCode::absent, "Missing receiver is distinct from timeout");
    for (const auto* bad : {"AULA_DEVICE 1 ready", "AULA_DEVICE 1 ready -1", "AULA_DEVICE 1 ready 101",
        "AULA_DEVICE 1 ready 20 extra", "AULA_DEVICE 1 present 95", "AULA_DEVICE 2 ready 95", "AULA_DEVICE 1 unknown",
        "AULA_DEVICE 1 ready 95\nAULA_DEVICE 1 absent", "95%", "AULA_DEVICE 1 busy extra"})
        require(!device_reply(bad), "Reject malformed, unsupported or ambiguous frame");
    using namespace std::chrono_literals;
    using Phase = DeviceState::Phase;
    const auto t = DeviceState::Time{};
    DeviceState state(t); BatteryState battery(t);
    require(state.probe_due(t) && !state.settled() && !state.can_apply(), "Startup checks before allowing writes");
    state.start_probe(); require(!state.probe_due(t), "No overlapping probe");
    state.finish_probe(DeviceReply{DeviceCode::absent, {}}, t);
    require(state.phase() == Phase::receiver_missing && state.settled() && !state.query_requested(), "Startup without receiver still completes");
    require(!state.probe_due(t + 4s) && state.probe_due(t + 5s), "Receiver detection retry interval");
    state.finish_probe(DeviceReply{DeviceCode::present, {}}, t + 5s);
    require(state.query_requested() && !state.can_apply(), "Arrival schedules a fresh communication check");
    const auto first = state.start_query(); battery.start();
    state.finish_query(DeviceReply{DeviceCode::ready, 95}, first); battery.finish(95, t + 6s);
    require(state.phase() == Phase::responding && state.can_apply(), "Ready after validated reply");
    state.finish_probe(DeviceReply{DeviceCode::present, {}}, t + 10s);
    require(!state.query_requested(), "Read-only probes do not trigger unnecessary battery queries");
    state.finish_probe(DeviceReply{DeviceCode::absent, {}}, t + 15s); battery.invalidate();
    require(!state.can_apply() && battery.percent() == 95 && battery.phase(t + 15s) == BatteryState::Phase::stale, "Removal disables writes and retains old battery");
    state.finish_probe(DeviceReply{DeviceCode::present, {}}, t + 20s);
    const auto second = state.start_query();
    state.finish_query(DeviceReply{DeviceCode::no_response, {}}, second);
    require(state.phase() == Phase::no_response && state.presence() == DeviceState::Presence::present, "No response does not claim an unplugged or sleeping keyboard");
    state.finish_query(DeviceReply{DeviceCode::incompatible, {}}, state.start_query());
    require(state.phase() == Phase::incompatible && !state.can_apply(), "Incompatible descriptor blocks UI writes");
    state.finish_query(DeviceReply{DeviceCode::busy, {}}, state.start_query());
    require(state.phase() == Phase::busy, "Busy is distinguishable");
    state.finish_query(DeviceReply{DeviceCode::ready, {}}, state.start_query());
    require(state.phase() == Phase::error, "An incomplete ready result is never reported as communication success");
    state.finish_query(DeviceReply{DeviceCode::ready, 0}, state.start_query()); battery.finish(0, t + 25s);
    require(state.phase() == Phase::responding && battery.percent() == 0, "Recovery preserves valid zero");
    const auto old = state.start_query();
    state.finish_probe(DeviceReply{DeviceCode::absent, {}}, t + 30s);
    state.finish_probe(DeviceReply{DeviceCode::present, {}}, t + 35s);
    require(!state.finish_query(DeviceReply{DeviceCode::ready, 99}, old) && state.query_requested(), "Discard a query from an older receiver generation");
    state.finish_probe({}, t + 40s);
    require(state.phase() == Phase::error && !state.can_apply(), "Failed detector does not imply a receiver is present");
    DeviceState broken(t); broken.finish_probe({}, t);
    require(broken.settled(), "Startup cannot hang on helper failure");
    const auto usbReady = device_reply("AULA_DEVICE 2 usb ready\n");
    require(usbReady && usbReady->transport == ControlTransport::usb && !usbReady->percent, "USB ready must not fabricate a battery value");
    require(device_reply("AULA_DEVICE 2 2g ready 95\n")->percent == 95, "Version 2 receiver battery");
    for (const auto* bad : {"AULA_DEVICE 2 usb ready 100", "AULA_DEVICE 2 bt ready 90", "AULA_DEVICE 2 2g ready",
            "AULA_DEVICE 2 usb present extra", "AULA_DEVICE 2 usb ready\nAULA_DEVICE 1 ready 95"})
        require(!device_reply(bad), "Reject mixed frames, invented USB battery and unimplemented transports");
    DeviceState switched(t);
    switched.finish_probe(DeviceReply{DeviceCode::present, {}}, t);
    const auto receiverQuery = switched.start_query();
    switched.finish_probe(DeviceReply{DeviceCode::present, {}, ControlTransport::usb}, t + 1s);
    require(switched.transport() == ControlTransport::usb && switched.query_requested() && !switched.can_apply(), "Switching to USB requires channel verification despite retained receiver");
    require(!switched.finish_query(DeviceReply{DeviceCode::ready, 95}, receiverQuery), "Late receiver reading cannot verify the new USB channel");
    switched.finish_query(usbReady, switched.start_query());
    require(switched.can_apply() && switched.phase() == Phase::responding, "Verified USB enables lighting without a battery query");
    switched.finish_probe(DeviceReply{DeviceCode::present, {}, ControlTransport::usb}, t + 5s);
    require(!switched.query_requested(), "Unchanged USB does not schedule battery traffic");
    switched.finish_query(DeviceReply{DeviceCode::busy, {}, ControlTransport::usb}, switched.start_query());
    require(!switched.can_apply(), "Unverified/busy USB never enables lighting");
    switched.finish_probe(DeviceReply{DeviceCode::present, {}, ControlTransport::usb}, t + 5s);
    require(switched.query_requested(), "USB verification recovers from temporary access failure");
    switched.finish_query(usbReady, switched.start_query());
    const auto usbQuery = switched.start_query();
    switched.finish_probe(DeviceReply{DeviceCode::present, {}}, t + 6s);
    require(!switched.finish_query(usbReady, usbQuery) && switched.query_requested(), "USB removal invalidates late USB results");
    switched.finish_query(DeviceReply{DeviceCode::ready, 42}, switched.start_query());
    require(switched.can_apply() && switched.transport() == ControlTransport::receiver, "Receiver control recovers after removing cable");
    switched.finish_query(usbReady, switched.start_query());
    require(!switched.can_apply(), "A mismatched helper transport never enables writes");
    std::cout << "Device frames, startup, presence, reconnect, stale data and failure recovery OK\n";
}
