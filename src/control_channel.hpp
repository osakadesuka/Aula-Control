#pragma once
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class ControlTransport { receiver, usb };
inline const char* transport_name(ControlTransport value) { return value == ControlTransport::usb ? "usb" : "2g"; }
inline std::optional<ControlTransport> parse_transport(std::string_view value) {
    if (value == "usb") return ControlTransport::usb;
    if (value == "2g") return ControlTransport::receiver;
    return {};
}
struct ControlEndpoint {
    std::string path;
    ControlTransport transport;
};
struct EndpointSelection {
    std::optional<ControlEndpoint> endpoint;
    bool ambiguous = false;
    ControlTransport transport = ControlTransport::receiver;
};
inline std::optional<ControlTransport> control_candidate(unsigned vid, unsigned pid, int interfaceNumber,
        unsigned page, unsigned usage, std::wstring_view product, bool usbBus) {
    if (!usbBus) return {};
    if (vid == 0x05ac && pid == 0x024f && interfaceNumber == 3 && page == 0xff60 && usage == 0x61)
        return ControlTransport::receiver;
    // 0C45:8006 is reused by unrelated keyboards. Product + interface + exact
    // descriptor verification are required for the observed EA75 variant.
    if (vid == 0x0c45 && pid == 0x8006 && interfaceNumber == 2 && page == 0xff13 && usage == 1
        && product == L"EPOMAKER EA75") return ControlTransport::usb;
    return {};
}
inline EndpointSelection select_control_endpoint(const std::vector<ControlEndpoint>& endpoints) {
    EndpointSelection result;
    for (const auto& endpoint : endpoints) if (endpoint.transport == ControlTransport::usb) result.transport = ControlTransport::usb;
    for (const auto& endpoint : endpoints) {
        if (endpoint.transport != result.transport) continue;
        if (result.endpoint && result.endpoint->path == endpoint.path) continue;
        if (result.endpoint) { result.endpoint.reset(); result.ambiguous = true; return result; }
        result.endpoint = endpoint;
    }
    return result;
}
inline bool control_descriptor_matches(ControlTransport transport, const unsigned char* data, std::size_t size) {
    constexpr std::array<unsigned char, 34> receiver{0x06,0x60,0xff,0x09,0x61,0xa1,0x01,0x09,0x62,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x20,0x81,0x02,0x09,0x63,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x20,0x91,0x02,0xc0};
    constexpr std::array<unsigned char, 48> usb{0x06,0x13,0xff,0x09,0x01,0xa1,0x01,0x09,0x02,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x40,0x81,0x02,0x09,0x03,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x40,0x91,0x02,0x0a,0x00,0xff,0x15,0x00,0x26,0xff,0x00,0x75,0x08,0x95,0x40,0xb1,0x02,0xc0};
    if (!data) return false;
    const auto equal = [&](const auto& expected) {
        if (size != expected.size()) return false;
        for (std::size_t i = 0; i < size; ++i) if (data[i] != expected[i]) return false;
        return true;
    };
    return transport == ControlTransport::usb ? equal(usb) : equal(receiver);
}
