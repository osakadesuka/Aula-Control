#include "preview_effects.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace {
constexpr std::array<int, 18> modes{{1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19}};
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
bool near(double a, double b, double tolerance = 1e-5) { return std::abs(a - b) <= tolerance; }
bool same(const PreviewLight& a, const PreviewLight& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.level == b.level;
}
PreviewLight light(int mode, double time, float col, float row, int index = 0, bool multi = false, float power = 1) {
    return preview_effect_light(mode, multi, 0.8f, 0.2f, 0.1f, power, time, index, col, row);
}
double distance(const PreviewLight& a, const PreviewLight& b) {
    return std::abs(a.r - b.r) + std::abs(a.g - b.g) + std::abs(a.b - b.b) + std::abs(a.level - b.level);
}
float row_peak(int mode, double time, int row) {
    float maximum = 0;
    for (int col = 0; col <= 64; ++col)
        maximum = (std::max)(maximum, light(mode, time, col / 4.0f, static_cast<float>(row)).level);
    return maximum;
}
float column_peak(int mode, double time, float row) {
    float level = -1, position = -1;
    for (int col = 0; col <= 160; ++col) {
        const auto sample = light(mode, time, col / 10.0f, row);
        if (sample.level > level) { level = sample.level; position = col / 10.0f; }
    }
    return position;
}
}

int main() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    for (const int mode : modes) for (const bool multi : {false, true}) {
        for (double time : {0.0, 0.033, 0.5, 1.3, 3.8, 7.5, 99.99, 1e9, 1e12, 1e100,
                (std::numeric_limits<double>::max)(), -1.0, nan, infinity}) {
            for (int index = 0; index < 82; ++index) {
                const float col = static_cast<float>(index % 17), row = static_cast<float>(index % 6);
                const auto a = light(mode, time, col, row, index, multi);
                const auto b = light(mode, time, col, row, index, multi);
                require(same(a,b), "Simulation must be deterministic at the same time/key");
                for (const float value : {a.r,a.g,a.b,a.level})
                    require(std::isfinite(value) && value >= 0 && value <= 1, "All 18 effects return finite normalized output");
                const auto dark = light(mode,time,col,row,index,multi,0);
                require(dark.level == 0, "Zero intensity produces no light in any palette");
                require(near(light(mode,time,col,row,index,multi,.25f).level,a.level*.25), "Intensity is applied once");
                if (!multi && mode != 6 && mode != 8)
                    require(a.r == .8f && a.g == .2f && a.b == .1f, "Single-color effects preserve selected RGB");
            }
        }
    }
    const auto sanitized = preview_effect_light(11,true,static_cast<float>(nan),-10,10,static_cast<float>(infinity),
        nan,0,static_cast<float>(infinity),static_cast<float>(nan));
    require(std::isfinite(sanitized.r) && sanitized.level == 0, "Invalid numeric input is bounded");
    require(light(3,1,4,2).level == 0 && light(20,1,4,2).level == 0, "Unknown modes do not invent a supported effect");

    for (const int mode : {1,6}) for (const bool multi : {false,true})
        for (int index = 0; index < 82; ++index)
            require(same(light(mode,0,5,2,index,multi),light(mode,100001.2,5,2,index,multi)), "Static palettes never animate");
    require(distance(light(6,0,1,1,1),light(6,0,1,1,2)) > .3, "Confetti uses different static colors per key");
    require(distance(light(1,0,0,2,0,true),light(1,0,12,2,0,true)) > .5, "Fixed rainbow spans the keyboard spatially");
    require(same(light(8,2,0,0),light(8,2,16,5)), "Gradual color transition is uniform across keys");
    require(distance(light(8,2,8,2),light(8,3,8,2)) > .3, "Built-in color cycle changes hue over time");
    require(distance(light(8,8.99999,8,2),light(8,9.00001,8,2)) < .001, "Hue cycle crosses its seam smoothly");
    require(light(7,0,2,1).level < .001 && light(7,2.4,2,1).level > .999, "Breathing covers dark to fully lit");
    require(near(light(7,1.1,1,1).level,light(7,1.1,15,5).level), "Breathing brightness stays in phase across the keyboard");

    for (double t : {.5,1.0,1.5}) {
        int lit = 0;
        for (int index=0; index<82; ++index) if (light(2,t,0,0,index).level > .001) ++lit;
        require(lit > 0 && lit <= 3, "Individual reactive demo illuminates only a few keys");
    }
    for (double t : {.3,1.75,3.2,4.65}) {
        int rows = 0;
        for (int row=0; row<6; ++row) {
            const float level = light(13,t,0,static_cast<float>(row)).level;
            require(near(level,light(13,t,16,static_cast<float>(row)).level), "Row reaction lights the whole row together");
            if (level > .01) ++rows;
        }
        require(rows == 1, "Row reaction selects exactly one demonstrated row");
    }
    for (int row=0; row<6; ++row) {
        const double t = row * 1.25 + .625;
        for (int other=0; other<6; ++other)
            require(other == row ? row_peak(16,t,other) > .9f : row_peak(16,t,other) == 0, "Cascade completes rows in order");
    }
    require(column_peak(16,.25,0) < column_peak(16,.9,0), "Cascade travels horizontally inside the active row");
    require(column_peak(11,.6,2) < column_peak(11,1.4,2), "Horizontal wave travels left to right");
    require(light(10,.5,7,1).level > light(10,.5,7,4).level
        && light(10,2.0,7,4).level > light(10,2.0,7,1).level, "Vertical wave travels top to bottom");
    for (const int mode : {10,11}) for (double t : {0.0,.137,.65,1.49,2.62,3.81,1e9}) {
        std::array<bool,6> spectrum{};
        // Inspect visible chroma across the entire board, not just the brightest
        // band: red/green/blue and their yellow/cyan/magenta transitions.
        const int samples = mode == 10 ? 6 : 65;
        for (int i=0; i<samples; ++i) {
            const auto sample = light(mode,t,mode == 10 ? 8.f : i*.25f,
                mode == 10 ? static_cast<float>(i) : 2.f,0,true,.6f);
            require(near(sample.level,.6), "Rainbow waves keep both halves and every row visibly illuminated");
            spectrum[0] = spectrum[0] || (sample.r > .99f && sample.g < .55f && sample.b < .55f);
            spectrum[1] = spectrum[1] || (sample.g > .99f && sample.r < .55f && sample.b < .55f);
            spectrum[2] = spectrum[2] || (sample.b > .99f && sample.r < .55f && sample.g < .55f);
            spectrum[3] = spectrum[3] || (sample.r > .45f && sample.g > .45f && sample.b < .05f);
            spectrum[4] = spectrum[4] || (sample.g > .45f && sample.b > .45f && sample.r < .05f);
            spectrum[5] = spectrum[5] || (sample.r > .45f && sample.b > .45f && sample.g < .05f);
        }
        for(const bool visible : spectrum) require(visible, "The full rainbow remains visible simultaneously at every tested phase");
    }
    for (double t : {.2,1.3,3.13,3.19,5.9}) for(float x : {1.f,6.f,13.f}) {
        require(distance(light(11,t,x,2,0,true),light(11,t+.12,x+.6f,2,0,true)) < 1e-5,
            "Horizontal rainbow hues travel right at the monochrome wave speed, including the phase seam");
        require(distance(light(10,t,8,1,0,true),light(10,t+.25,8,1.f+static_cast<float>(.25*7.5/3.8),0,true)) < 1e-5,
            "Vertical rainbow hues travel down at the monochrome wave speed");
    }
    require(distance(light(11,3.2-1e-6,8,2,0,true),light(11,3.2+1e-6,8,2,0,true)) < 1e-4
        && distance(light(10,3.04-1e-6,8,2,0,true),light(10,3.04+1e-6,8,2,0,true)) < 1e-4,
        "Rainbow wave loops have no hue or brightness jump");
    for (float offset : {1.f,3.f,5.f,7.f})
        require(near(light(9,1.3,8-offset,2.5f).level,light(9,1.3,8+offset,2.5f).level), "Center wave expands symmetrically");
    require(column_peak(19,.5,0) < column_peak(19,1.3,0)
        && column_peak(19,.5,1) > column_peak(19,1.3,1), "Neighboring rows travel in opposite directions");
    require(light(18,0,0,5).level > .99 && light(18,0,16,0).level < .02
        && light(18,1.44,16,0).level > .99, "Diagonal wave starts at bottom left and reaches top right");
    require(light(14,2.8,8,2).level == 0 && light(15,3.0,8,2).level == 0, "Reactive sweeps leave a quiet interval between demonstrations");
    for (double t : {.5,1.3,2.0}) for(float x : {1.f,4.f,7.f})
        require(near(light(17,t,x,3).level,light(17,t,16-x,3).level), "Center whip has mirrored arms");

    for (int mode : modes) {
        if (mode == 1 || mode == 6) continue;
        double motion = 0;
        for (int frame=0; frame<8; ++frame) for(int index=0; index<82; ++index) {
            const float x=static_cast<float>(index%17), y=static_cast<float>(index%6);
            motion += distance(light(mode,frame*.57,x,y,index,true),light(mode,frame*.57+.22,x,y,index,true));
        }
        require(motion > 1.0, "Every animated effect changes visibly over time");
    }
    for (int mode : {7,8,9,10,11,12,18,19}) {
        for(int index=0; index<82; ++index) {
            const float x=static_cast<float>(index%17), y=static_cast<float>(index%6);
            require(distance(light(mode,1e9,x,y,index,true),light(mode,1e9+1.0/30.0,x,y,index,true)) < .3,
                "Old persisted times retain smooth frame-to-frame motion");
        }
    }
    std::cout << "18 preview effects: deterministic palettes, bounds, power, directions, row sequencing and long timelines OK\n";
}
