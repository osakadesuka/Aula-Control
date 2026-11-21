#include "media_knob.hpp"
#include <iostream>
#include <cstdlib>
#include <array>
#include <string>
#include <vector>

namespace {
// Live diagnosis observes only multimedia keys; normal typing is never logged.
std::array<std::atomic<unsigned>, 6> physical{}, injected{};
struct MediaEdge { ULONGLONG elapsed; DWORD eventTime, key; bool down, injected, own; };
std::array<MediaEdge, 1024> edges{};
std::size_t edgeCount = 0;
ULONGLONG observeStarted = 0;
LRESULT CALLBACK observe_media(int code, WPARAM message, LPARAM data) {
    if (code == HC_ACTION && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP)) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
        int index = -1;
        if (key->vkCode == VK_VOLUME_MUTE) index = 0;
        else if (key->vkCode == VK_VOLUME_DOWN) index = 1;
        else if (key->vkCode == VK_VOLUME_UP) index = 2;
        else if (key->vkCode == VK_MEDIA_PLAY_PAUSE) index = 3;
        else if (key->vkCode == VK_MEDIA_NEXT_TRACK) index = 4;
        else if (key->vkCode == VK_MEDIA_PREV_TRACK) index = 5;
        const bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        if (index >= 0) {
            if (down) ++((key->flags & LLKHF_INJECTED) ? injected : physical)[index];
            if (observeStarted && edgeCount < edges.size())
                edges[edgeCount++] = {GetTickCount64() - observeStarted, key->time, key->vkCode, down,
                    (key->flags & LLKHF_INJECTED) != 0, key->dwExtraInfo == kMediaKnobInputTag};
        }
    }
    return CallNextHookEx(nullptr, code, message, data);
}
int observe_only() {
    observeStarted = GetTickCount64();
    HHOOK observer = SetWindowsHookExW(WH_KEYBOARD_LL, observe_media, GetModuleHandleW(nullptr), 0);
    if (!observer) return 3;
    const auto timer = SetTimer(nullptr, 0, 90000, nullptr);
    if (!timer) { UnhookWindowsHookEx(observer); return 4; }
    std::cout << "OBSERVATION_READY: 90 seconds; media edges only; no remapping or synthetic input.\n" << std::flush;
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TIMER && message.wParam == timer) break;
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    KillTimer(nullptr, timer); UnhookWindowsHookEx(observer);
    for (std::size_t i = 0; i < edgeCount; ++i) {
        const auto& edge = edges[i];
        std::cout << edge.elapsed << " ms event=" << edge.eventTime << " key=" << edge.key << ' '
            << (edge.down ? "down" : "up") << " injected=" << edge.injected << " own=" << edge.own << '\n';
    }
    std::cout << "MEDIA_EDGES=" << edgeCount << '\n';
    return edgeCount == edges.size() ? 5 : 0;
}
int diagnose() {
    MediaKnob knob;
    const bool started = knob.start();
    std::cout << "Remap installed: " << started << std::endl;
    if (!started) return 2;
    HHOOK observer = SetWindowsHookExW(WH_KEYBOARD_LL, observe_media, GetModuleHandleW(nullptr), 0);
    if (!observer) return 3;
    const auto timer = SetTimer(nullptr, 0, 30000, nullptr);
    if (!timer) { UnhookWindowsHookEx(observer); return 4; }
    std::cout << "Listening to multimedia keys for 30 seconds..." << std::endl;
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TIMER && message.wParam == timer) break;
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    KillTimer(nullptr, timer); UnhookWindowsHookEx(observer); knob.stop();
    const char* names[]{"Mute", "VolumeDown", "VolumeUp", "PlayPause", "NextTrack", "PreviousTrack"};
    for (int i = 0; i < 6; ++i) std::cout << names[i] << ": physical=" << physical[i] << " injected=" << injected[i] << '\n';
    std::cout << "Remapped=" << knob.presses() << " Failures=" << knob.failures() << std::endl;
    return 0;
}
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--observe") return observe_only();
    if (argc > 1 && std::string(argv[1]) == "--diagnose") return diagnose();
    using Action = MediaAction;
    using Tick = std::uint64_t;
    std::vector<Action> sent;
    MediaClickState state;
    const auto require = [](bool ok) { if (!ok) { std::cerr << "Media click state test failed\n"; std::exit(1); } };
    auto emit = [&](Action action) { sent.push_back(action); return true; };
    const auto press = [&](Tick at, Tick length = 30) {
        require(state.handle(VK_VOLUME_MUTE, true, 1234, at, emit));
        require(state.handle(VK_VOLUME_MUTE, false, 1234, at + length, emit));
    };
    require(media_key(Action::play_pause) == VK_MEDIA_PLAY_PAUSE && media_key(Action::next_track) == VK_MEDIA_NEXT_TRACK
        && media_key(Action::previous_track) == VK_MEDIA_PREV_TRACK && media_key(Action::none) == 0);
    require(!state.wait_ms(0));
    for (const auto key : std::array<DWORD, 6>{{'A', VK_VOLUME_UP, VK_VOLUME_DOWN, VK_MEDIA_PLAY_PAUSE, VK_MEDIA_NEXT_TRACK, VK_MEDIA_PREV_TRACK}})
        require(!state.handle(key, true, 0, 0, emit));
    require(!state.handle(VK_VOLUME_MUTE, true, kMediaKnobInputTag, 0, emit));
    require(!state.handle(VK_VOLUME_MUTE, false, 0, 0, emit));
    press(1000);
    require(sent.empty() && state.wait_ms(1030) == 400u);
    require(!state.flush(1429, emit));
    require(state.flush(1430, emit) && sent == std::vector<Action>{Action::play_pause});
    require(!state.flush(2000, emit) && !state.wait_ms(2000));
    // A double click must emit only Next, never an early Play/Pause.
    sent.clear(); press(3000); press(3200);
    require(sent.empty() && !state.flush(3629, emit));
    require(state.flush(3630, emit) && sent == std::vector<Action>{Action::next_track});
    sent.clear(); press(4000); press(4200); press(4400);
    require(sent == std::vector<Action>{Action::previous_track});
    require(!state.flush(4830, emit) && sent == std::vector<Action>{Action::previous_track});
    // More clicks are capped to one Previous, rather than Previous + Play/Pause.
    sent.clear(); for (Tick t = 5000; t <= 5600; t += 200) press(t);
    require(!state.flush(6030, emit) && sent == std::vector<Action>{Action::previous_track});
    // Auto-repeat, tagged output and an old timer while the next press is held.
    sent.clear(); press(7000);
    require(state.handle(VK_VOLUME_MUTE, true, 0, 7400, emit));
    require(state.handle(VK_VOLUME_MUTE, true, 0, 7450, emit));
    require(!state.handle(VK_VOLUME_MUTE, false, kMediaKnobInputTag, 7460, emit));
    require(!state.flush(7480, emit) && !state.wait_ms(7480));
    require(state.handle(VK_VOLUME_MUTE, false, 0, 7500, emit));
    require(state.flush(7900, emit) && sent == std::vector<Action>{Action::next_track});
    // A press at the exact boundary completes the previous burst first.
    sent.clear(); press(9000);
    require(state.handle(VK_VOLUME_MUTE, true, 0, 9430, emit));
    require(sent == std::vector<Action>{Action::play_pause});
    require(state.handle(VK_VOLUME_MUTE, false, 0, 9460, emit));
    require(state.flush(9860, emit) && sent == std::vector<Action>({Action::play_pause, Action::play_pause}));
    // Holding the knob to change its physical mode must not issue media actions.
    sent.clear(); press(10000); press(10200, 4000);
    require(!state.flush(15000, emit) && sent.empty());
    state.cancel(); press(16000); state.cancel();
    require(!state.flush(17000, emit) && sent.empty());
    require(state.handle(VK_VOLUME_MUTE, true, 0, 18000, emit)); state.cancel();
    require(!state.handle(VK_VOLUME_MUTE, false, 0, 18030, emit));
    // Suspend or a stalled message loop must not play stale commands on resume.
    press(19000);
    require(!state.flush(60000, emit) && sent.empty());
    // Failed output is attempted once and no delayed duplicates remain.
    press(61000); unsigned attempts = 0;
    const auto fail = [&](Action) { ++attempts; return false; };
    require(!state.flush(61430, fail) && !state.flush(62000, fail) && attempts == 1 && !state.wait_ms(62000));
    // The clock stays correct past the 32-bit tick rollover.
    sent.clear(); press((Tick{1} << 32) + 1000);
    require(state.flush((Tick{1} << 32) + 1430, emit) && sent == std::vector<Action>{Action::play_pause});
    // Complete duplicated pairs, rather than keydown auto-repeat, caused the
    // observed single -> Next fault. They must not extend the real deadline.
    state.cancel(); sent.clear(); press(30000); press(30046, 16); press(30080, 0);
    require(sent.empty() && state.wait_ms(30080) == 350u);
    require(!state.flush(30429, emit));
    require(state.flush(30430, emit) && sent == std::vector<Action>{Action::play_pause});
    state.cancel(); sent.clear(); press(31000, 0); press(31099, 0); press(31100, 0);
    require(state.flush(31500, emit) && sent == std::vector<Action>{Action::next_track});
    // Cancelling resets the guard too, including a press at clock value zero.
    state.cancel(); sent.clear(); press(0, 0); state.cancel(); press(20, 0);
    require(state.flush(420, emit) && sent == std::vector<Action>{Action::play_pause});
    // Replay the actual USB observation, including the 16-ms duplicate fifth
    // single and intentional double/triple presses spaced roughly 250–350 ms.
    state.cancel(); sent.clear();
    const std::array<std::array<Tick, 2>, 34> observed{{
        {30625,30641},{33047,33047},{35297,35297},{37484,37500},{39562,39578},{39578,39578},
        {43484,43500},{43828,43844},{45578,45578},{45906,45922},{47703,47719},{48000,48000},
        {49734,49734},{50031,50031},{51656,51656},{51969,51984},
        {53953,53953},{54219,54234},{54531,54531},{55937,55953},{56250,56250},{56562,56578},
        {58297,58312},{58562,58578},{58875,58875},{60547,60562},{60812,60828},{61125,61141},
        {62781,62797},{63125,63125},{63391,63391},{65031,65047},{65328,65344},{65625,65625}
    }};
    Tick clock = 0;
    const auto advance = [&](Tick target) {
        for (;;) {
            const auto wait = state.wait_ms(clock);
            if (!wait || clock + *wait > target) break;
            clock += *wait; state.flush(clock, emit);
        }
        clock = target;
    };
    for (const auto& pair : observed) {
        advance(pair[0]); require(state.handle(VK_VOLUME_MUTE, true, 0, clock, emit));
        advance(pair[1]); require(state.handle(VK_VOLUME_MUTE, false, 0, clock, emit));
    }
    advance(clock + 1000);
    std::vector<Action> expected(5, Action::play_pause);
    expected.insert(expected.end(), 5, Action::next_track); expected.insert(expected.end(), 6, Action::previous_track);
    require(sent == expected);
    state.cancel(); unsigned tripleAttempts = 0;
    const auto failTriple = [&](Action action) { require(action == Action::previous_track); ++tripleAttempts; return false; };
    for (Tick t = 70000; t <= 70600; t += 200) {
        require(state.handle(VK_VOLUME_MUTE, true, 0, t, failTriple));
        require(state.handle(VK_VOLUME_MUTE, false, 0, t + 10, failTriple));
    }
    require(!state.flush(71010, failTriple) && tripleAttempts == 1);
    std::cout << "Single/double/triple presses, timing boundaries, repeat/long-press suppression, cancellation and failure recovery OK\n";
}
