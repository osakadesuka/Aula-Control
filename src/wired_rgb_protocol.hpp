#pragma once
#include "rgb_protocol.hpp"

// EA75 USB: 64-byte feature payloads, with report ID 0 prepended by hidapi.
// This is a separate protocol from the receiver's 32-byte output reports.
using WiredRgbReport = std::array<unsigned char, 65>;
struct WiredRgbPackets { WiredRgbReport begin{}, select{}, effect{}, save{}, finish{}; };
inline std::optional<WiredRgbPackets> wired_rgb_packets(const RgbParameters& settings) {
    if (!rgb_packets(settings)) return {};
    WiredRgbPackets packets;
    packets.begin[1] = 4; packets.begin[2] = 0x18;
    packets.select[1] = 4; packets.select[2] = 0x13; packets.select[9] = 1;
    packets.effect[1] = static_cast<unsigned char>(settings.mode);
    for (std::size_t i = 0; i < settings.color.size(); ++i) packets.effect[i + 2] = settings.color[i];
    packets.effect[9] = settings.multicolor ? 1 : 0;
    packets.effect[10] = static_cast<unsigned char>(settings.brightness);
    packets.effect[11] = static_cast<unsigned char>(settings.speed);
    packets.effect[15] = 0xaa; packets.effect[16] = 0x55;
    packets.save[1] = 4; packets.save[2] = 2;
    packets.finish[1] = 4; packets.finish[2] = 0xf0;
    return packets;
}
inline bool wired_rgb_ack(const WiredRgbReport& reply, unsigned char command) {
    return reply[0] == 0 && reply[1] == 4 && reply[2] == command && reply[3] == 0 && reply[4] == 1;
}
