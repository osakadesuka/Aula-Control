#pragma once
#include <algorithm>
#include <chrono>

// Only the simulated lighting and startup spinner use this budget. Raven keeps
// its own timing for input, hover and page transitions.
class PreviewCadence {
public:
    using Clock = std::chrono::steady_clock;
    double wait_seconds(bool animated, bool background, Clock::time_point now) const {
        if (background) return .25;
        if (!animated) return .05;
        return std::clamp(std::chrono::duration<double>(next_ - now).count(), 0.0, .05);
    }
    bool due(bool animated, Clock::time_point now) {
        if (!animated) { next_ = now; return false; }
        if (now < next_) return false;
        next_ = now + std::chrono::nanoseconds(33333334); // At most 30 requested updates/s.
        return true;
    }
private:
    Clock::time_point next_{};
};
