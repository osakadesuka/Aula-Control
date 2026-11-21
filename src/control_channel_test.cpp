#include "control_channel.hpp"
#include "wired_rgb_protocol.hpp"
#include <cstdlib>
#include <iostream>

void require(bool value, const char* reason) { if (!value) { std::cerr << reason << '\n'; std::exit(1); } }
int main() {
    using T = ControlTransport;
    require(control_candidate(0x0c45, 0x8006, 2, 0xff13, 1, L"EPOMAKER EA75", true) == T::usb, "Observed wired identity");
    require(!control_candidate(0x0c45, 0x8006, 2, 0xff13, 1, L"Other keyboard", true), "Shared VID/PID is insufficient");
    require(!control_candidate(0x0c45, 0x8006, 2, 0xff13, 1, L"EPOMAKER EA75", false), "USB control must not claim Bluetooth");
    require(!control_candidate(0x0c45, 0x8006, 1, 0xffff, 1, L"EPOMAKER EA75", true), "Never open the media/keyboard collection as control");
    require(control_candidate(0x05ac, 0x024f, 3, 0xff60, 0x61, L"EA75 Dongle", true) == T::receiver, "Keep receiver support");
    require(!control_candidate(0x05ac, 0x024f, 4, 0xff60, 0x61, L"EA75 Dongle", true), "Unknown receiver interface stays excluded");
    require(!select_control_endpoint({}).endpoint, "Absent endpoint");
    const auto both = select_control_endpoint({{"dongle", T::receiver}, {"cable", T::usb}});
    require(both.endpoint && both.endpoint->path == "cable" && both.transport == T::usb, "Cable wins while dongle remains plugged in");
    require(select_control_endpoint({{"cable", T::usb}, {"dongle", T::receiver}}).endpoint->path == "cable", "Selection is independent of enumeration order");
    require(select_control_endpoint({{"cable", T::usb}, {"other cable", T::usb}, {"dongle", T::receiver}}).ambiguous, "Ambiguous USB must not fall back to dongle");
    require(select_control_endpoint({{"dongle", T::receiver}, {"other dongle", T::receiver}}).ambiguous, "Ambiguous receivers block writes");
    require(select_control_endpoint({{"cable", T::usb}, {"cable", T::usb}}).endpoint.has_value(), "Duplicate enumeration of same path is harmless");
    require(select_control_endpoint({{"dongle", T::receiver}}).endpoint->transport == T::receiver, "Unplugging cable selects the receiver again");
    const std::array<unsigned char, 48> descriptor{0x06,0x13,0xff,0x09,0x01,0xa1,0x01,0x09,0x02,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x40,0x81,0x02,0x09,0x03,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x40,0x91,0x02,0x0a,0x00,0xff,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x40,0xb1,0x02,0xc0};
    require(control_descriptor_matches(T::usb, descriptor.data(), descriptor.size()), "Descriptor captured from user's USB interface");
    require(!control_descriptor_matches(T::receiver, descriptor.data(), descriptor.size())
        && !control_descriptor_matches(T::usb, descriptor.data(), descriptor.size() - 1), "Wrong transport and truncated descriptors fail closed");
    auto changed = descriptor; changed[17] = 0x20;
    require(!control_descriptor_matches(T::usb, changed.data(), changed.size()), "Report sizes cannot be silently substituted");
    const auto packets = wired_rgb_packets({{0x12, 0x34, 0x56}, 11, 5, 2, true});
    require(packets.has_value(), "Valid wired settings");
    WiredRgbReport expected{}; expected[1] = 4; expected[2] = 0x18;
    require(packets->begin == expected, "USB session frame includes zero report ID and no receiver checksum");
    expected = {}; expected[1] = 4; expected[2] = 0x13; expected[9] = 1;
    require(packets->select == expected, "Only lighting section selected");
    expected = {}; expected[1] = 11; expected[2] = 0x12; expected[3] = 0x34; expected[4] = 0x56;
    expected[9] = 1; expected[10] = 5; expected[11] = 2; expected[15] = 0xaa; expected[16] = 0x55;
    require(packets->effect == expected, "Whole wired RGB frame, palette, brightness, speed and zero padding");
    expected = {}; expected[1] = 4; expected[2] = 2;
    require(packets->save == expected, "Save stage");
    expected[2] = 0xf0; require(packets->finish == expected, "Exit stage");
    expected = {}; expected[1] = 4; expected[2] = 0x18; expected[4] = 1;
    require(wired_rgb_ack(expected, 0x18) && !wired_rgb_ack(expected, 0x13), "ACK must match the current command");
    expected[4] = 0; require(!wired_rgb_ack(expected, 0x18), "Echo is not a positive ACK");
    require(!wired_rgb_packets({{255, 0, 0}, 21, 3, 3, false})
        && !wired_rgb_packets({{255, 0, 0}, 1, 6, 3, false})
        && !wired_rgb_packets({{255, 0, 0}, 1, 3, 0, false}), "Invalid settings never build packets");
    std::cout << "USB identity, arbitration, descriptor guards and wired RGB framing OK\n";
}
