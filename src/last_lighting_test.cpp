#include "last_lighting.hpp"
#include <cstdlib>
#include <iostream>

void require(bool value, const char* reason) { if (!value) { std::cerr << reason << '\n'; std::exit(1); } }
int main() {
    const auto receipt = lighting_receipt("Diagnostic\nAULA_LIGHTING 1 usb 1795212345678\r\n");
    require(receipt && receipt->transport == ControlTransport::usb && receipt->timeMs == 1795212345678LL, "Complete timed receipt");
    require(lighting_receipt("AULA_LIGHTING 1 2g 1234\n")->transport == ControlTransport::receiver, "Receiver receipt");
    for (const auto* text : {"AULA_LIGHTING 1 usb 0", "AULA_LIGHTING 1 usb -1", "AULA_LIGHTING 2 usb 1234",
            "AULA_LIGHTING 1 bt 1234", "AULA_LIGHTING 1 usb 1234 extra", "AULA_LIGHTING 1 usb",
            "AULA_LIGHTING 1 usb 9223372036854775808", "AULA_LIGHTING 1 usb 1234\nAULA_LIGHTING 1 2g 1234", "Settings sent"})
        require(!lighting_receipt(text), "Malformed or ambiguous receipts do not update memory");
    const auto parse = [](const char* text) { std::istringstream input(text); return read_last_lighting(input); };
    const auto sample = parse("AULA_LAST_LIGHTING 1\n12 63 240 19 25 1 5 2\nusb 1795212345678\n");
    require(sample && sample->settings == LightingSettings{12,63,240,19,25,true,5,2}
        && sample->receipt.transport == ControlTransport::usb && sample->receipt.timeMs == 1795212345678LL, "All lighting parameters survive serialization");
    for (const auto* text : {"AULA_LAST_LIGHTING 2\nfuture", "AULA_LAST_LIGHTING 1\n12 63 240 20 25 1 5 2\nusb 1234",
            "AULA_LAST_LIGHTING 1\n256 63 240 19 25 1 5 2\nusb 1234", "AULA_LAST_LIGHTING 1\n12 63 240 19 25 2 5 2\nusb 1234",
            "AULA_LAST_LIGHTING 1\n12 63 240 19 25 1 5 2\nusb 0", "AULA_LAST_LIGHTING 1\n12 63 240 19 25 1 5 2\nusb 1234\nextra",
            "AULA_LAST_LIGHTING 1\n12 63 240 19 25 1 5\nusb 1234"}) require(!parse(text), "Reject corrupt or future memory");
    const auto directory = std::filesystem::temp_directory_path() / (L"AulaControl-last-lighting-test-" + std::to_wstring(GetCurrentProcessId()));
    const auto path = directory / L"last-lighting.txt";
    LastLightingStore store(path);
    require(!store.load() && store.readable(), "First run has no invented applied effect");
    require(store.save(*sample), "Persist only the explicitly supplied completed send");
    const auto restored = LastLightingStore(path).load();
    require(restored && restored->settings == sample->settings && restored->receipt.timeMs == sample->receipt.timeMs, "Restore after a new process/store instance");
    LastLighting invalid = *sample; invalid.settings.speed = 6;
    require(!store.save(invalid) && LastLightingStore(path).load()->settings == sample->settings, "Invalid draft cannot replace applied memory");
    { std::ofstream output(path, std::ios::trunc); output << "AULA_LAST_LIGHTING 99\nfuture\n"; }
    LastLightingStore future(path);
    require(!future.load() && !future.readable() && !future.save(*sample), "Future file is never overwritten");
    { std::ifstream input(path); std::string header; std::getline(input, header); require(header == "AULA_LAST_LIGHTING 99", "Keep unreadable file intact"); }
    std::error_code ec; std::filesystem::remove(path, ec); std::filesystem::remove(directory, ec);
    LightingRestorePolicy restore;
    require(!restore.due(false,true,true,1) && !restore.due(true,false,true,1) && !restore.due(true,true,false,1), "Restore requires preference, saved data and healthy connection");
    require(restore.due(true,true,true,1), "Restore on initial connection"); restore.claim(1);
    require(!restore.due(true,true,true,1), "No repeated writes or automatic retries after an attempted send");
    require(restore.due(true,true,true,2), "Reconnect gets one new restore attempt"); restore.claim(2);
    require(!restore.due(true,true,true,2), "Manual send can claim the same connection too");
    using namespace std::chrono_literals;
    const auto t = LightingTimeline::Clock::time_point{};
    LightingTimeline timeline(t);
    require(timeline.sample(t+2s,true) == 2 && timeline.sample(t+2s,true) == 2, "Two views sample one clock without advancing twice");
    require(timeline.sample(t+10s,false) == 2, "Disabled animations freeze phase");
    timeline.set_speed(5,t+10s,false);
    require(timeline.sample(t+13s,true) == 7, "Speed changes preserve phase and affect subsequent time");
    timeline.restart(3, .5, t+20s);
    require(timeline.sample(t+21s,true) == 1.5, "Successful send anchors phase to its receipt, including helper latency");
    require(timeline.sample(t+31s,true) == 11.5, "Returning from tray accounts for elapsed time without rendering in background");
    require(lighting_elapsed_seconds(1000,1500) == .5 && lighting_elapsed_seconds(1500,1000) == 0, "Clamp clock changes safely");
    std::cout << "Lighting receipts, atomic memory, recovery, restore policy and preview timeline OK\n";
}
