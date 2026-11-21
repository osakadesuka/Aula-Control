#pragma once
#include <hidapi.h>
#include "control_channel.hpp"

inline EndpointSelection enumerate_control_endpoint() {
    std::vector<ControlEndpoint> endpoints;
    const auto collect = [&](unsigned short vid, unsigned short pid) {
        auto* head = hid_enumerate(vid, pid);
        for (auto* d = head; d; d = d->next) {
            const auto transport = control_candidate(d->vendor_id, d->product_id, d->interface_number,
                d->usage_page, d->usage, d->product_string ? d->product_string : L"", d->bus_type == HID_API_BUS_USB);
            if (transport && d->path) endpoints.push_back({d->path, *transport});
        }
        hid_free_enumeration(head);
    };
    // Keep periodic discovery targeted; do not inspect every unrelated HID.
    collect(0x0c45, 0x8006);
    if (endpoints.empty()) collect(0x05ac, 0x024f);
    return select_control_endpoint(endpoints);
}
inline bool verify_control_endpoint(hid_device* handle, ControlTransport transport) {
    std::array<unsigned char, HID_API_MAX_REPORT_DESCRIPTOR_SIZE> descriptor{};
    const auto size = hid_get_report_descriptor(handle, descriptor.data(), descriptor.size());
    return size > 0 && control_descriptor_matches(transport, descriptor.data(), static_cast<std::size_t>(size));
}
