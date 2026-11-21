#pragma once
#include <array>
#include <optional>

using RgbPacket = std::array<unsigned char, 32>;
struct RgbParameters {
    std::array<unsigned char, 3> color{};
    int mode = 1, brightness = 3, speed = 3;
    bool multicolor = false;
};
struct RgbPackets { RgbPacket commit{}, effect{}; };

inline void rgb_checksum(RgbPacket& packet) {
    unsigned sum = 0;
    for (std::size_t i = 0; i < 31; ++i) sum += packet[i];
    packet[31] = static_cast<unsigned char>(sum);
}

// The color order is R/G/B as observed on this keyboard, independently of
// other variants. Speed and palette use the same 32-byte command family.
inline std::optional<RgbPackets> rgb_packets(const RgbParameters& settings) {
    if (settings.mode < 1 || settings.mode > 20 || settings.brightness < 1 || settings.brightness > 5
        || settings.speed < 1 || settings.speed > 5) return {};
    RgbPackets packets;
    packets.commit[0] = 0x0f; rgb_checksum(packets.commit);
    auto& effect = packets.effect;
    effect[0] = 0x05; effect[1] = 0x10; effect[3] = static_cast<unsigned char>(settings.mode);
    effect[4] = settings.color[0]; effect[5] = settings.color[1]; effect[6] = settings.color[2];
    effect[11] = settings.multicolor ? 1 : 0;
    effect[12] = static_cast<unsigned char>(settings.brightness);
    effect[13] = static_cast<unsigned char>(settings.speed);
    effect[17] = 0xaa; effect[18] = 0x55; rgb_checksum(effect);
    return packets;
}
