#pragma once
#include <charconv>
#include <chrono>
#include <optional>
#include <string_view>
#include "control_channel.hpp"

enum class DeviceCode { present, absent, ambiguous, ready, busy, incompatible, write_error, read_error, no_response, error };
inline const char* device_code_name(DeviceCode code) {
    switch (code) {
    case DeviceCode::present: return "present";
    case DeviceCode::absent: return "absent";
    case DeviceCode::ambiguous: return "ambiguous";
    case DeviceCode::ready: return "ready";
    case DeviceCode::busy: return "busy";
    case DeviceCode::incompatible: return "incompatible";
    case DeviceCode::write_error: return "write_error";
    case DeviceCode::read_error: return "read_error";
    case DeviceCode::no_response: return "no_response";
    default: return "error";
    }
}
struct DeviceReply { DeviceCode code; std::optional<int> percent; ControlTransport transport = ControlTransport::receiver; };

// A complete, versioned frame is required; human diagnostics cannot establish
// receiver presence or overwrite a last valid battery reading.
inline std::optional<DeviceReply> device_reply(std::string_view output) {
    constexpr std::string_view family = "AULA_DEVICE ", prefix = "AULA_DEVICE 1 ", prefix2 = "AULA_DEVICE 2 ";
    std::optional<DeviceReply> result;
    while (!output.empty()) {
        const auto end = output.find('\n');
        auto line = output.substr(0, end);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.substr(0, family.size()) == family) {
            if (result) return {};
            ControlTransport transport = ControlTransport::receiver;
            if (line.substr(0, prefix2.size()) == prefix2) {
                line.remove_prefix(prefix2.size());
                const auto separator = line.find(' ');
                const auto parsed = parse_transport(line.substr(0, separator));
                if (!parsed || separator == std::string_view::npos) return {};
                transport = *parsed; line.remove_prefix(separator + 1);
            } else {
                if (line.substr(0, prefix.size()) != prefix) return {};
                line.remove_prefix(prefix.size());
            }
            bool known = false;
            for (auto code : {DeviceCode::present, DeviceCode::absent, DeviceCode::ambiguous, DeviceCode::ready,
                DeviceCode::busy, DeviceCode::incompatible, DeviceCode::write_error, DeviceCode::read_error, DeviceCode::no_response, DeviceCode::error}) {
                const auto name = std::string_view(device_code_name(code));
                if (line.substr(0, name.size()) != name) continue;
                if (code == DeviceCode::ready) {
                    if (transport == ControlTransport::usb) {
                        if (line != name) return {};
                        result = DeviceReply{code, {}, transport}; known = true; break;
                    }
                    if (line.size() <= name.size() + 1 || line[name.size()] != ' ') return {};
                    const auto number = line.substr(name.size() + 1);
                    int value = -1;
                    const auto parsed = std::from_chars(number.data(), number.data() + number.size(), value);
                    if (parsed.ec != std::errc{} || parsed.ptr != number.data() + number.size() || value < 0 || value > 100) return {};
                    result = DeviceReply{code, value, transport}; known = true; break;
                }
                if (line != name) return {};
                result = DeviceReply{code, {}, transport}; known = true; break;
            }
            if (!known) return {};
        }
        if (end == std::string_view::npos) break;
        output.remove_prefix(end + 1);
    }
    return result;
}

class DeviceState {
public:
    using Clock = std::chrono::steady_clock;
    using Time = Clock::time_point;
    enum class Presence { unknown, present, absent, ambiguous, error };
    enum class Phase { checking, receiver_missing, responding, no_response, ambiguous, busy, incompatible, error };
    explicit DeviceState(Time now) : nextProbe_(now) {}
    bool probe_due(Time now) const { return !probing_ && now >= nextProbe_; }
    void start_probe() { probing_ = true; }
    void finish_probe(std::optional<DeviceReply> reply, Time now) {
        probing_ = false; nextProbe_ = now + std::chrono::seconds(5);
        Presence next = Presence::error;
        if (reply) {
            if (reply->code == DeviceCode::present) next = Presence::present;
            else if (reply->code == DeviceCode::absent) next = Presence::absent;
            else if (reply->code == DeviceCode::ambiguous) next = Presence::ambiguous;
        }
        const auto nextTransport = reply ? reply->transport : transport_;
        const bool changedTransport = nextTransport != transport_;
        const bool arrived = next == Presence::present && (presence_ != Presence::present || changedTransport);
        if (changedTransport) { transport_ = nextTransport; ++generation_; }
        set_presence(next);
        if (arrived) { queryRequested_ = true; lastQuery_ = DeviceCode::present; }
        if (next == Presence::present && transport_ == ControlTransport::usb && lastQuery_ != DeviceCode::ready)
            queryRequested_ = true; // Retry descriptor/open failures on the next 5-second probe.
        if (next != Presence::present) { queryRequested_ = false; settled_ = true; }
    }
    bool query_requested() const { return presence_ == Presence::present && queryRequested_; }
    unsigned start_query() { querying_ = true; queryRequested_ = false; return generation_; }
    bool finish_query(std::optional<DeviceReply> reply, unsigned generation) {
        querying_ = false;
        if (generation != generation_) return false;
        if (reply && reply->transport != transport_) {
            lastQuery_ = DeviceCode::incompatible; settled_ = true; return false;
        }
        if (reply && reply->code == DeviceCode::ready && (transport_ == ControlTransport::usb
                ? reply->percent.has_value() : (!reply->percent || *reply->percent < 0 || *reply->percent > 100))) reply.reset();
        settled_ = true;
        lastQuery_ = reply && reply->code != DeviceCode::present ? reply->code : DeviceCode::error;
        if (lastQuery_ == DeviceCode::absent) set_presence(Presence::absent);
        else if (lastQuery_ == DeviceCode::ambiguous) set_presence(Presence::ambiguous);
        return true;
    }
    Presence presence() const { return presence_; }
    ControlTransport transport() const { return transport_; }
    unsigned generation() const { return generation_; }
    bool settled() const { return settled_; }
    bool can_apply() const {
        return presence_ == Presence::present && !querying_
            && (transport_ == ControlTransport::usb ? lastQuery_ == DeviceCode::ready
                : lastQuery_ != DeviceCode::present && lastQuery_ != DeviceCode::incompatible);
    }
    Phase phase() const {
        if (presence_ == Presence::absent) return Phase::receiver_missing;
        if (presence_ == Presence::ambiguous) return Phase::ambiguous;
        if (presence_ == Presence::error) return Phase::error;
        if (presence_ == Presence::unknown || querying_ || lastQuery_ == DeviceCode::present) return Phase::checking;
        if (lastQuery_ == DeviceCode::ready) return Phase::responding;
        if (lastQuery_ == DeviceCode::no_response) return Phase::no_response;
        if (lastQuery_ == DeviceCode::busy) return Phase::busy;
        if (lastQuery_ == DeviceCode::incompatible) return Phase::incompatible;
        return Phase::error;
    }
private:
    void set_presence(Presence value) { if (value != presence_) { presence_ = value; ++generation_; } }
    Presence presence_ = Presence::unknown;
    ControlTransport transport_ = ControlTransport::receiver;
    DeviceCode lastQuery_ = DeviceCode::present;
    Time nextProbe_;
    unsigned generation_ = 0;
    bool probing_ = false, querying_ = false, queryRequested_ = false, settled_ = false;
};
