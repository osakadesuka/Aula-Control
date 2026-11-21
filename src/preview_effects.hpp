#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Pure visual simulation: no HID, keyboard capture, wall clock or mutable state.
// RGB is normalized chroma; level includes the supplied intensity exactly once.
struct PreviewLight { float r, g, b, level; };

namespace preview_effect_detail {
inline constexpr double pi = 3.14159265358979323846;
inline double unit(double x) { return std::isfinite(x) ? std::clamp(x, 0.0, 1.0) : 0.0; }
inline double wrap(double x, double period) {
    const double p = std::fmod(x, period);
    return p < 0.0 ? p + period : p;
}
inline double smooth(double low, double high, double x) {
    const double p = unit((x - low) / (high - low));
    return p * p * (3.0 - 2.0 * p);
}
inline double bell(double distance, double width) {
    const double d = distance / width;
    return std::abs(d) < 3.5 ? std::exp(-0.5 * d * d) : 0.0;
}
inline double spot(double dx, double dy, double width, double height) {
    const double d = dx * dx / (width * width) + dy * dy / (height * height);
    return d < 12.25 ? std::exp(-0.5 * d) : 0.0;
}
inline double cyclic_distance(double distance, double span) {
    return wrap(distance + span * 0.5, span) - span * 0.5;
}
inline std::uint32_t mix(std::uint32_t x) {
    x ^= x >> 16; x *= 0x7feb352du;
    x ^= x >> 15; x *= 0x846ca68bu;
    return x ^ (x >> 16);
}
inline std::uint32_t cycle_id(double cycle) {
    // Reduce before conversion, so an old persisted time never overflows an int.
    if (!std::isfinite(cycle)) return 0;
    return static_cast<std::uint32_t>(wrap(cycle, 4294967296.0));
}
inline double noise(std::uint32_t seed) { return mix(seed) / 4294967296.0; }
inline PreviewLight hue(double h, float level) {
    const double sector = wrap(h, 1.0) * 6.0;
    const int n = static_cast<int>(sector);
    const float f = static_cast<float>(sector - n);
    constexpr float low = 0.035f;
    const float up = low + (1.0f - low) * f;
    const float down = 1.0f - (1.0f - low) * f;
    switch (n) {
    case 0: return {1, up, low, level};
    case 1: return {down, 1, low, level};
    case 2: return {low, 1, up, level};
    case 3: return {low, down, 1, level};
    case 4: return {up, low, 1, level};
    default: return {1, low, down, level};
    }
}
inline double tap_envelope(double age, double life) {
    return smooth(0.0, 0.045, age) * (1.0 - smooth(0.07, life, age));
}
} // namespace preview_effect_detail

// time is already scaled by LightingTimeline (3/5 is the baseline speed).
// col/row are key CENTERS in key units: left..right 0..16, top..bottom 0..5.
// Row-reactive demonstrations group centers by nearest integer row. index is
// the stable visual key index, 0..81, and is never a hardware LED/key code.
inline PreviewLight preview_effect_light(int mode, bool multicolor, float red, float green,
        float blue, float intensity, double time, int index, float col, float row) {
    using namespace preview_effect_detail;
    const double t = std::isfinite(time) && time > 0.0 ? time : 0.0;
    const double x = std::isfinite(col) ? std::clamp(static_cast<double>(col), 0.0, 16.0) : 0.0;
    const double y = std::isfinite(row) ? std::clamp(static_cast<double>(row), 0.0, 5.0) : 0.0;
    const int line = static_cast<int>(std::floor(y + 0.5));
    const auto key = static_cast<std::uint32_t>(index);
    double mask = 1.0, color = x / 18.0 + y * 0.025;
    const bool palette = multicolor || mode == 6 || mode == 8;

    switch (mode) {
    case 1: // A fixed spatial rainbow also remains still when time advances.
        break;
    case 2: { // Demonstrated individual taps: a few keys, followed by a decay.
        constexpr double interval = 0.48, lifetime = 1.15;
        const double event = std::floor(t / interval);
        mask = 0.0;
        for (int previous = 0; previous < 4; ++previous) {
            const double number = event - previous;
            if (number < 0.0) continue;
            const auto seed = mix(cycle_id(number) + 0x9e3779b9u);
            if (index < 0 || index >= 82 || seed % 82u != key) continue;
            const double light = tap_envelope(t - number * interval, lifetime);
            if (light > mask) { mask = light; color = noise(seed ^ 0xc2b2ae35u); }
        }
        break;
    }
    case 4: { // Independently fading dark patches; no shared sine flicker.
        constexpr double period = 2.4;
        const double local = t + noise(key + 0x31a2u) * period;
        const auto cycle = cycle_id(std::floor(local / period));
        const double age = wrap(local, period);
        if (noise(mix(key + 1u) ^ mix(cycle + 0xade1u)) < 0.58)
            mask = 1.0 - 0.99 * smooth(0.0, 0.22, age) * (1.0 - smooth(0.80, 1.65, age));
        break;
    }
    case 5: { // Short falling droplets with tails, faster than random fade.
        constexpr double interval = 0.19;
        const double event = std::floor(t / interval);
        mask = 0.0;
        for (int previous = 0; previous < 7; ++previous) {
            const double number = event - previous;
            if (number < 0.0) continue;
            const auto seed = mix(cycle_id(number) + 0x68bc21ebu);
            const double head = -0.8 + (t - number * interval) * 7.2;
            const double column = noise(seed) * 16.0;
            const double behind = head - y;
            const double tail = behind >= 0.0 ? std::exp(-behind * 2.1) : bell(behind, 0.27);
            const double light = bell(x - column, 0.58) * tail;
            if (light > mask) { mask = light; color = noise(seed ^ 0x85ebca6bu); }
        }
        break;
    }
    case 6: // Built-in confetti ignores the single-color selection.
        color = noise(key + 0xb5297a4du);
        break;
    case 7: { // Every key breathes together, retaining its spatial hue.
        const double breath = 0.5 - 0.5 * std::cos(wrap(t, 4.8) * (2.0 * pi / 4.8));
        mask = breath * breath * (3.0 - 2.0 * breath);
        break;
    }
    case 8: // Uniform, continuous hue transition with the built-in palette.
        color = wrap(t, 9.0) / 9.0;
        break;
    case 9: { // Elliptical wavefronts expand from the center toward both sides.
        const double dx = x - 8.0, dy = (y - 2.5) * 1.25;
        const double radius = std::sqrt(dx * dx + dy * dy);
        const double travel = wrap(t, 3.8) / 3.8 * 10.8;
        mask = bell(cyclic_distance(radius - travel, 10.8), 1.15);
        color = radius / 12.0 - wrap(t, 9.0) / 9.0;
        break;
    }
    case 10: { // Top-to-bottom bands.
        const double travel = wrap(t, 3.8) / 3.8 * 7.5;
        mask = multicolor ? 1.0 : bell(cyclic_distance(y - travel, 7.5), 0.85);
        // Rainbow is a travelling spectrum over all six rows, not a narrow
        // brightness band hiding most hues. Match the monochrome band speed:
        // 6 rows / 3.04 seconds = 7.5 rows / 3.8 seconds, moving downward.
        color = y / 6.0 - wrap(t, 3.04) / 3.04;
        break;
    }
    case 11: { // Left-to-right bands.
        const double travel = wrap(t, 3.8) / 3.8 * 19.0;
        mask = multicolor ? 1.0 : bell(cyclic_distance(x - travel, 19.0), 2.1);
        // One full visible spectrum across the board. Subtracting time makes
        // each hue move right at 5 key units/s, just like the monochrome band.
        color = x / 16.0 - wrap(t, 3.2) / 3.2;
        break;
    }
    case 12: { // Two curved spiral arms rotate around the keyboard center.
        const double dx = x - 8.0, dy = (y - 2.5) * 1.35;
        const double radius = std::sqrt(dx * dx + dy * dy);
        const double angle = std::atan2(dy, dx);
        const double phase = angle + radius * 0.48 - wrap(t, 4.8) * (2.0 * pi / 4.8);
        const double arm = 0.5 + 0.5 * std::cos(phase * 2.0);
        mask = arm * arm * arm;
        color = angle / (2.0 * pi) + radius * 0.035 - wrap(t, 9.0) / 9.0;
        break;
    }
    case 13: { // Demonstrated row taps: the selected row lights as one group.
        constexpr double interval = 1.45;
        const auto seed = mix(cycle_id(std::floor(t / interval)) + 0x91e10da5u);
        mask = line == static_cast<int>(seed % 6u) ? tap_envelope(wrap(t, interval), 1.10) : 0.0;
        break;
    }
    case 14: { // A crisp horizontal sweep follows each demonstrated tap.
        const double age = wrap(t, 3.4);
        const double front = -1.5 + age * 10.6;
        mask = bell(x - front, 0.88) * (1.0 - smooth(1.7, 2.1, age));
        color -= wrap(t, 10.0) / 10.0;
        break;
    }
    case 15: { // A softer wave spreads from a demonstrated key and fades out.
        constexpr double interval = 3.6;
        const auto seed = mix(cycle_id(std::floor(t / interval)) + 0x74dcb303u);
        const double originX = 3.0 + noise(seed) * 10.0;
        const double originY = 1.0 + noise(seed ^ 0x27d4eb2du) * 3.0;
        const double dx = x - originX, dy = (y - originY) * 1.25;
        const double distance = std::sqrt(dx * dx + dy * dy);
        const double age = wrap(t, interval);
        mask = bell(distance - age * 6.0, 1.65) * smooth(0.0, 0.09, age) * (1.0 - smooth(1.5, 2.5, age));
        color -= wrap(t, 10.0) / 10.0;
        break;
    }
    case 16: { // Finish one horizontal row before starting the next row.
        constexpr double rowTime = 1.25;
        const double cycle = wrap(t, rowTime * 6.0);
        const int active = static_cast<int>(cycle / rowTime);
        const double age = wrap(cycle, rowTime);
        mask = line == active ? bell(x - (-1.3 + age / rowTime * 18.6), 1.05) : 0.0;
        color -= wrap(t, 10.0) / 10.0;
        break;
    }
    case 17: { // Mirrored curved whips originate at the top center.
        constexpr double period = 3.2;
        const double age = wrap(t, period);
        mask = 0.0;
        for (int trail = 0; trail < 12; ++trail) {
            const double progress = (age - trail * 0.045) / 2.65;
            if (progress < 0.0 || progress > 1.0) continue;
            const double offset = std::sin(progress * pi * 0.5) * 8.0;
            const double headY = -0.4 + progress * 6.0;
            const double envelope = (1.0 - trail / 13.0) * smooth(0.0, 0.06, progress)
                * (1.0 - smooth(0.88, 1.0, progress));
            const double left = spot(x - (8.0 - offset), y - headY, 0.80, 0.48);
            const double right = spot(x - (8.0 + offset), y - headY, 0.80, 0.48);
            mask = (std::max)(mask, (std::max)(left, right) * envelope);
        }
        color = std::abs(x - 8.0) / 11.0 + y * 0.025 - wrap(t, 10.0) / 10.0;
        break;
    }
    case 18: { // Fast diagonal: bottom-left Ctrl toward top-right Delete.
        const double projection = (x / 16.0 + (5.0 - y) / 5.0) * 0.5;
        const double front = wrap(t, 1.8) / 1.8 * 1.25;
        mask = bell(cyclic_distance(projection - front, 1.25), 0.085);
        color = projection - wrap(t, 6.0) / 6.0;
        break;
    }
    case 19: { // Neighboring rows travel in opposite horizontal directions.
        const double along = line % 2 == 0 ? x : 16.0 - x;
        const double front = wrap(t, 3.8) / 3.8 * 19.0;
        mask = bell(cyclic_distance(along - front, 19.0), 1.8);
        color = along / 18.0 - wrap(t, 8.0) / 8.0;
        break;
    }
    default: // Uncharacterized firmware modes are not invented in the preview.
        mask = 0.0;
        break;
    }

    const float level = static_cast<float>(unit(intensity) * unit(mask));
    if (palette) return hue(color, level);
    return {static_cast<float>(unit(red)), static_cast<float>(unit(green)), static_cast<float>(unit(blue)), level};
}
