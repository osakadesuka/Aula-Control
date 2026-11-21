#include "preview_cadence.hpp"
#include <cstdlib>
#include <iostream>
static void require(bool ok, const char* reason) { if (!ok) { std::cerr << reason << '\n'; std::exit(1); } }
int main() {
    PreviewCadence cadence;
    auto now = PreviewCadence::Clock::time_point{} + std::chrono::seconds(1);
    require(cadence.due(true, now), "First visible frame is immediate");
    require(!cadence.due(true, now + std::chrono::milliseconds(1)), "Input events do not trigger extra preview frames");
    const auto wait = cadence.wait_seconds(true, false, now);
    require(wait > .033 && wait < .034, "Wait follows the next frame deadline");
    unsigned frames = 1;
    for (int ms = 1; ms < 1000; ++ms) frames += cadence.due(true, now + std::chrono::milliseconds(ms));
    require(frames == 30, "Preview requests remain bounded with frequent wakeups");
    now += std::chrono::seconds(10);
    require(cadence.due(true, now) && !cadence.due(true, now), "A stall does not replay missed frames");
    require(!cadence.due(false, now), "Hidden or static preview does not repaint");
    require(cadence.wait_seconds(false, true, now) == .25 && cadence.wait_seconds(false, false, now) == .05, "Background waits and static responsiveness are preserved");
    require(cadence.due(true, now), "Restoring starts without a stale deadline");
}
