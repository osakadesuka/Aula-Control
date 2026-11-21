#pragma once
#include "media_clicks.hpp"
#include <atomic>
#include <future>
#include <thread>
#include <utility>

// Global opt-in Mute gestures; this hook cannot identify the source device.
// Gesture state, timers and SendInput belong only to the worker thread.
class MediaKnob {
public:
    struct Feedback { unsigned actions, failures; MediaAction lastAction; };
    ~MediaKnob() { stop(); }
    bool start() {
        if (worker_.joinable()) return true;
        stopping_.store(false); state_.cancel();
        std::promise<bool> ready;
        auto result = ready.get_future();
        worker_ = std::thread([this, ready = std::move(ready)]() mutable {
            MSG msg{};
            PeekMessageW(&msg, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
            threadId_ = GetCurrentThreadId();
            HANDLE unique = CreateMutexW(nullptr, FALSE, L"Local\\AulaControlMuteToPlayPause");
            if (!unique || GetLastError() == ERROR_ALREADY_EXISTS) {
                if (unique) CloseHandle(unique);
                ready.set_value(false); return;
            }
            owner_ = this;
            HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, callback, GetModuleHandleW(nullptr), 0);
            if (!hook) { owner_ = nullptr; CloseHandle(unique); ready.set_value(false); return; }
            ready.set_value(true);
            while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
                if (stopping_.load()) continue;
                if (msg.message == WM_TIMER && timer_ && msg.wParam == timer_) {
                    KillTimer(nullptr, timer_); timer_ = 0;
                    state_.flush(GetTickCount64(), [this](MediaAction action) { return emit(action); });
                    arm_timer();
                } else { TranslateMessage(&msg); DispatchMessageW(&msg); }
            }
            if (timer_) { KillTimer(nullptr, timer_); timer_ = 0; }
            state_.cancel(); UnhookWindowsHookEx(hook);
            owner_ = nullptr; CloseHandle(unique);
        });
        if (result.get()) return true;
        worker_.join(); threadId_ = 0; return false;
    }
    void stop() {
        if (!worker_.joinable()) return;
        stopping_.store(true); // Cancel a pending burst when disabling or exiting.
        PostThreadMessageW(threadId_, WM_QUIT, 0, 0);
        worker_.join(); threadId_ = 0;
    }
    Feedback feedback() const {
        const auto packed = completed_.load();
        return {static_cast<unsigned>(packed >> 8), failures_.load(), static_cast<MediaAction>(packed & 255)};
    }
    unsigned presses() const { return feedback().actions; }
    unsigned failures() const { return failures_.load(); }
private:
    void arm_timer() {
        if (timer_) { KillTimer(nullptr, timer_); timer_ = 0; }
        const auto wait = state_.wait_ms(GetTickCount64());
        if (wait && !stopping_.load()) {
            timer_ = SetTimer(nullptr, 0, *wait, nullptr);
            if (!timer_) { state_.cancel(); ++failures_; }
        }
    }
    bool emit(MediaAction action) {
        if (stopping_.load()) return false;
        INPUT inputs[2]{};
        for (auto& input : inputs) {
            input.type = INPUT_KEYBOARD; input.ki.wVk = media_key(action);
            input.ki.dwExtraInfo = kMediaKnobInputTag;
        }
        inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
        const UINT sent = SendInput(2, inputs, sizeof(INPUT));
        if (sent != 2) {
            ++failures_;
            if (sent == 1) SendInput(1, &inputs[1], sizeof(INPUT));
            return false;
        }
        const auto count = (completed_.load() >> 8) + 1;
        completed_.store((count << 8) | static_cast<unsigned>(action));
        return true;
    }
    static LRESULT CALLBACK callback(int code, WPARAM message, LPARAM data) {
        if (code == HC_ACTION && owner_ && !owner_->stopping_.load()
            && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN || message == WM_KEYUP || message == WM_SYSKEYUP)) {
            const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
            const bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
            if (owner_->state_.handle(key->vkCode, pressed, key->dwExtraInfo, GetTickCount64(),
                                     [](MediaAction action) { return owner_->emit(action); })) {
                owner_->arm_timer(); return 1;
            }
        }
        return CallNextHookEx(nullptr, code, message, data);
    }
    inline static thread_local MediaKnob* owner_ = nullptr;
    MediaClickState state_;
    std::thread worker_;
    DWORD threadId_ = 0;
    UINT_PTR timer_ = 0;
    std::atomic<bool> stopping_{false};
    std::atomic<std::uint64_t> completed_{0};
    std::atomic<unsigned> failures_{0};
};
