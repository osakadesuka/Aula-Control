#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>

inline constexpr ULONG_PTR kMediaKnobInputTag = 0x41554C41; // AULA
enum class MediaAction : unsigned { none, play_pause, next_track, previous_track };
inline constexpr WORD media_key(MediaAction action) {
    return action == MediaAction::play_pause ? VK_MEDIA_PLAY_PAUSE
        : action == MediaAction::next_track ? VK_MEDIA_NEXT_TRACK
        : action == MediaAction::previous_track ? VK_MEDIA_PREV_TRACK : 0;
}

// Monotonic milliseconds; the worker arms a timer only for a pending burst.
class MediaClickState {
public:
    static constexpr std::uint64_t clickGapMs = 400, maxShortPressMs = 600, maxLateMs = 1000;
    static constexpr std::uint64_t duplicateGuardMs = 100;
    template<class Emit> bool handle(DWORD key, bool pressed, ULONG_PTR extraInfo, std::uint64_t now, Emit emit) {
        if (key != VK_VOLUME_MUTE || extraInfo == kMediaKnobInputTag) return false;
        if (pressed) {
            if (!down_) {
                // Some knob clicks arrive as two complete down/up pairs only
                // 0–16 ms apart. Swallow both edges of the duplicate without
                // counting it or extending the original grouping deadline.
                duplicate_ = lastPressAt_ && now - *lastPressAt_ < duplicateGuardMs;
                if (!duplicate_) {
                    flush(now, emit); // Complete an overdue burst first.
                    lastPressAt_ = now;
                }
                down_ = true; pressedAt_ = now;
            }
            return true; // Auto-repeat never adds a click.
        }
        if (!down_) return false;
        down_ = false;
        const bool duplicate = duplicate_; duplicate_ = false;
        if (now - pressedAt_ > maxShortPressMs) { clicks_ = 0; tripleSent_ = false; return true; }
        if (duplicate) return true;
        if (clicks_ < 3) ++clicks_; // Four or more rapid presses produce one Previous.
        deadline_ = now + clickGapMs;
        // Three is the largest gesture: no fourth click can change its action.
        // Keep the burst latched until the gap ends, so extra edges cannot also
        // issue Play/Pause or repeat Previous.
        if (clicks_ == 3 && !tripleSent_) { tripleSent_ = true; emit(MediaAction::previous_track); }
        return true;
    }
    template<class Emit> bool flush(std::uint64_t now, Emit emit) {
        if (down_ || clicks_ == 0 || now < deadline_) return false;
        if (now - deadline_ > maxLateMs) { clicks_ = 0; tripleSent_ = false; return false; }
        if (tripleSent_) { clicks_ = 0; tripleSent_ = false; return false; }
        const auto action = clicks_ == 1 ? MediaAction::play_pause
            : clicks_ == 2 ? MediaAction::next_track : MediaAction::previous_track;
        clicks_ = 0; // A failed send must never repeat later.
        return emit(action);
    }
    std::optional<UINT> wait_ms(std::uint64_t now) const {
        if (down_ || clicks_ == 0) return {};
        return static_cast<UINT>(now < deadline_ ? deadline_ - now : 1);
    }
    void cancel() { down_ = false; duplicate_ = false; tripleSent_ = false; clicks_ = 0; lastPressAt_.reset(); }
private:
    bool down_ = false, duplicate_ = false, tripleSent_ = false;
    unsigned clicks_ = 0;
    std::uint64_t pressedAt_ = 0, deadline_ = 0;
    std::optional<std::uint64_t> lastPressAt_;
};
