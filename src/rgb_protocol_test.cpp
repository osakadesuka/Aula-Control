#include "rgb_protocol.hpp"
#include <cstdlib>
#include <iostream>

int main() {
    auto require = [](bool ok, const char* reason) { if (!ok) { std::cerr << reason << '\n'; std::exit(1); } };
    RgbParameters settings{{255, 0, 0}, 1, 3, 3, false};
    const auto original = rgb_packets(settings);
    require(original.has_value(), "Default command accepted");
    const RgbPacket observed{0x05,0x10,0x00,0x01,0xff,0,0,0,0,0,0,0,0x03,0x03,0,0,0,0xaa,0x55,0,0,0,0,0,0,0,0,0,0,0,0,0x1a};
    require(original->effect == observed, "Keep observed red command byte-for-byte");
    require(original->commit[0] == 0x0f && original->commit[31] == 0x0f, "Keep observed commit");
    for (int speed = 1; speed <= 5; ++speed) for (const bool multicolor : {false, true}) {
        settings.speed = speed; settings.multicolor = multicolor;
        const auto packets = rgb_packets(settings);
        require(packets && packets->commit == original->commit, "Parameters do not change commit");
        unsigned sum = 0;
        for (std::size_t i = 0; i < 31; ++i) {
            sum += packets->effect[i];
            if (i != 11 && i != 13) require(packets->effect[i] == observed[i], "Only palette and speed fields change");
        }
        require(packets->effect[11] == (multicolor ? 1 : 0) && packets->effect[13] == speed, "Encode independent parameters");
        require(packets->effect[31] == (sum & 255), "Checksum includes new parameters");
    }
    for (int invalid : {-1, 0, 6, 256}) { settings.speed = invalid; require(!rgb_packets(settings), "Reject unsupported speed"); }
    settings.speed = 3; settings.multicolor = true;
    const auto multicolor = rgb_packets(settings);
    for (int brightness = 1; brightness <= 5; ++brightness) {
        settings.brightness = brightness;
        const auto packets = rgb_packets(settings);
        require(packets && packets->commit == multicolor->commit, "Native brightness keeps the commit");
        unsigned sum = 0;
        for (std::size_t i = 0; i < 31; ++i) {
            sum += packets->effect[i];
            if (i != 12) require(packets->effect[i] == multicolor->effect[i], "Native brightness does not alter palette, speed or RGB");
        }
        require(packets->effect[12] == brightness && packets->effect[31] == (sum & 255), "Native brightness field and checksum");
    }
    for (int invalid : {-1, 0, 6, 256}) { settings.brightness = invalid; require(!rgb_packets(settings), "Reject unsupported native brightness"); }
    settings.brightness = 3;
    settings.speed = 3; settings.mode = 21; require(!rgb_packets(settings), "Reject unknown mode");
    settings.mode = 1; settings.brightness = 0; require(!rgb_packets(settings), "Reject unsupported brightness");
    std::cout << "Observed RGB vector, speed/palette isolation, checksum and validation OK\n";
}
