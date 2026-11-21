#pragma once
#include <raven.hpp>
#include <algorithm>
#include <cmath>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>
#include "preview_effects.hpp"

// Rectangles calibrated against assets/keyboard-actual-v1.png (1774 x 887).
// These are visual positions, not hardware LED or HID key indexes.
struct PreviewKey { float x, y, w, h; const char* legend; };
inline constexpr std::array<PreviewKey, 82> kPreviewKeys{{
    {80.0f, 134.0f, 102.0f, 103.0f, "Esc"},
    {205.0f, 134.0f, 97.0f, 103.0f, "F1"},
    {305.0f, 134.0f, 96.0f, 103.0f, "F2"},
    {404.0f, 134.0f, 95.0f, 103.0f, "F3"},
    {503.0f, 134.0f, 95.0f, 103.0f, "F4"},
    {622.0f, 134.0f, 95.0f, 103.0f, "F5"},
    {722.0f, 134.0f, 92.0f, 103.0f, "F6"},
    {818.0f, 134.0f, 92.0f, 103.0f, "F7"},
    {914.0f, 134.0f, 92.0f, 103.0f, "F8"},
    {1031.0f, 134.0f, 96.0f, 103.0f, "F9"},
    {1131.0f, 134.0f, 93.0f, 103.0f, "F10"},
    {1228.0f, 134.0f, 93.0f, 103.0f, "F11"},
    {1325.0f, 134.0f, 92.0f, 103.0f, "F12"},
    {1442.0f, 134.0f, 100.0f, 103.0f, "Del"},
    {84.0f, 267.0f, 94.0f, 96.0f, "~"},
    {181.8f, 267.0f, 94.0f, 96.0f, "1"},
    {279.6f, 267.0f, 94.0f, 96.0f, "2"},
    {377.4f, 267.0f, 94.0f, 96.0f, "3"},
    {475.2f, 267.0f, 94.0f, 96.0f, "4"},
    {573.0f, 267.0f, 94.0f, 96.0f, "5"},
    {670.8f, 267.0f, 94.0f, 96.0f, "6"},
    {768.6f, 267.0f, 94.0f, 96.0f, "7"},
    {866.4f, 267.0f, 94.0f, 96.0f, "8"},
    {964.2f, 267.0f, 94.0f, 96.0f, "9"},
    {1062.0f, 267.0f, 94.0f, 96.0f, "0"},
    {1159.8f, 267.0f, 94.0f, 96.0f, "-"},
    {1257.6f, 267.0f, 94.0f, 96.0f, "+"},
    {1358.0f, 267.0f, 184.0f, 96.0f, "Back"},
    {1592.0f, 267.0f, 96.0f, 96.0f, "Home"},
    {84.0f, 368.0f, 145.0f, 94.0f, "Tab"},
    {234.0f, 368.0f, 93.0f, 94.0f, "Q"},
    {331.3f, 368.0f, 93.0f, 94.0f, "W"},
    {428.6f, 368.0f, 93.0f, 94.0f, "E"},
    {525.9f, 368.0f, 93.0f, 94.0f, "R"},
    {623.2f, 368.0f, 93.0f, 94.0f, "T"},
    {720.5f, 368.0f, 93.0f, 94.0f, "Y"},
    {817.8f, 368.0f, 93.0f, 94.0f, "U"},
    {915.1f, 368.0f, 93.0f, 94.0f, "I"},
    {1012.4f, 368.0f, 93.0f, 94.0f, "O"},
    {1109.7f, 368.0f, 93.0f, 94.0f, "P"},
    {1207.0f, 368.0f, 93.0f, 94.0f, "["},
    {1304.3f, 368.0f, 93.0f, 94.0f, "]"},
    {1403.0f, 368.0f, 139.0f, 94.0f, "\\"},
    {1592.0f, 368.0f, 96.0f, 94.0f, "End"},
    {84.0f, 467.0f, 169.0f, 93.0f, "Caps"},
    {258.0f, 467.0f, 93.0f, 93.0f, "A"},
    {355.3f, 467.0f, 93.0f, 93.0f, "S"},
    {452.6f, 467.0f, 93.0f, 93.0f, "D"},
    {549.9f, 467.0f, 93.0f, 93.0f, "F"},
    {647.2f, 467.0f, 93.0f, 93.0f, "G"},
    {744.5f, 467.0f, 93.0f, 93.0f, "H"},
    {841.8f, 467.0f, 93.0f, 93.0f, "J"},
    {939.1f, 467.0f, 93.0f, 93.0f, "K"},
    {1036.4f, 467.0f, 93.0f, 93.0f, "L"},
    {1133.7f, 467.0f, 93.0f, 93.0f, ";"},
    {1231.0f, 467.0f, 93.0f, 93.0f, "'"},
    {1329.0f, 467.0f, 213.0f, 93.0f, "Enter"},
    {1592.0f, 467.0f, 96.0f, 93.0f, "PgUp"},
    {84.0f, 566.0f, 217.0f, 93.0f, "Shift"},
    {306.0f, 566.0f, 93.0f, 93.0f, "Z"},
    {403.5f, 566.0f, 93.0f, 93.0f, "X"},
    {501.0f, 566.0f, 93.0f, 93.0f, "C"},
    {598.5f, 566.0f, 93.0f, 93.0f, "V"},
    {696.0f, 566.0f, 93.0f, 93.0f, "B"},
    {793.5f, 566.0f, 93.0f, 93.0f, "N"},
    {891.0f, 566.0f, 93.0f, 93.0f, "M"},
    {988.5f, 566.0f, 93.0f, 93.0f, ","},
    {1086.0f, 566.0f, 93.0f, 93.0f, "."},
    {1183.5f, 566.0f, 93.0f, 93.0f, "/"},
    {1283.0f, 566.0f, 164.0f, 93.0f, "Shift"},
    {1593.0f, 566.0f, 95.0f, 93.0f, "PgDn"},
    {1474.0f, 579.0f, 94.0f, 94.0f, "Up"},
    {84.0f, 665.0f, 118.0f, 94.0f, "Ctrl"},
    {207.0f, 665.0f, 118.0f, 94.0f, "Win"},
    {331.0f, 665.0f, 114.0f, 94.0f, "Alt"},
    {451.0f, 665.0f, 607.0f, 94.0f, "Space"},
    {1064.0f, 665.0f, 90.0f, 94.0f, "Alt"},
    {1161.0f, 665.0f, 91.0f, 94.0f, "Fn"},
    {1259.0f, 665.0f, 91.0f, 94.0f, "Ctrl"},
    {1379.0f, 678.0f, 90.0f, 95.0f, "<"},
    {1477.0f, 678.0f, 91.0f, 95.0f, "Down"},
    {1576.0f, 678.0f, 90.0f, 95.0f, ">"}
}};

// Coordinates below are in the original product photograph, before scaling.
namespace keyboard_model {
constexpr int width = 1774, height = 887;
constexpr float margin = 14.0f;
constexpr float clamp01(float x) { return std::clamp(x, 0.0f, 1.0f); }

// Signed distance to the rounded footprint in the plate. Negative is inside.
inline float rim_distance(float x, float y, float w, float h, float radius) {
    const float dx = std::abs(x - w * .5f) - w * .5f + radius;
    const float dy = std::abs(y - h * .5f) - h * .5f + radius;
    const float ax = (std::max)(dx, 0.f), ay = (std::max)(dy, 0.f);
    return std::sqrt(ax * ax + ay * ay) + (std::min)((std::max)(dx, dy), 0.f) - radius;
}

template<class Sample>
raven::Texture make_mask(int w, int h, Sample alpha) {
    std::vector<uint8_t> pixels(static_cast<size_t>(w) * h * 4, 255);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            pixels[(static_cast<size_t>(y) * w + x) * 4 + 3] =
                static_cast<uint8_t>(255 * clamp01(alpha((x + .5f) / w, (y + .5f) / h)) + .5f);
        }
    }
    return raven::Texture::create_from_pixels(pixels.data(), w, h);
}

// Each key has a different photographed edge. Bake its light transmission once,
// from the surrounding well and the cap's own plastic, leaving the legend clear.
inline raven::Texture make_key_mask(const PreviewKey& k, const uint8_t* photo) {
    auto luminance = [&](float x, float y) {
        const int px = static_cast<int>(k.x + x), py = static_cast<int>(k.y + y);
        const auto* pixel = photo + 4 * (py * width + px);
        return (.21f * pixel[0] + .72f * pixel[1] + .07f * pixel[2]) / 255.f;
    };
    const float face = photo ? luminance(k.w * .71f, k.h * .36f) : .85f;
    const bool darkCap = face < .40f;
    const float w = k.w + 2 * margin, h = k.h + 2 * margin;
    return make_mask(static_cast<int>(std::ceil(w * .75f)), static_cast<int>(std::ceil(h * .75f)),
        [&](float u, float v) {
            const float x = u * w - margin, y = v * h - margin;
            const float d = rim_distance(x, y, k.w, k.h, 7.5f);
            if (d > margin || d < -20.f) return 0.f;
            const float lightness = photo ? luminance(x, y) : .48f;
            const float south = clamp01((y / k.h - .58f) * 2.5f);
            const float along = clamp01(x / k.w);
            const float lamp = std::sin(3.141593f * along);
            const float widthFalloff = 1.f / (1.f + std::abs(x - k.w*.5f) / 112.f);
            const float well = darkCap ? .82f : clamp01((face - lightness + .12f) * 1.75f);
            // Diode seen through the seam, broad indirect light on the sloping
            // south skirt, then a softer reflection on the neighbouring plate.
            const float edge = std::exp(-.5f * (d - .7f) * (d - .7f) / 7.f);
            const float spill = std::exp(-.5f * d * d / 75.f);
            const float skirt = std::exp(-.5f * (d + 6.f) * (d + 6.f) / 46.f);
            const float core = edge * (.16f + .80f * well) * (.40f + .60f * v);
            const float reflected = spill * (.07f + .12f*v) +
                skirt * south * (.16f + .84f*lamp) * (darkCap ? .13f : .46f);
            // Fade the sprite boundary fully, even where neighbouring lights overlap.
            return (core + reflected) * widthFalloff * clamp01((margin - d) * .25f) * clamp01((d + 20.f) * .25f);
        });
}

struct Model {
    raven::Texture photo, shadow, spill;
    std::array<raven::Texture, kPreviewKeys.size()> wells;

    explicit Model(const std::filesystem::path& path) {
        photo = raven::Texture::create_from_file(path.u8string().c_str());
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        int w = 0, h = 0;
        std::unique_ptr<uint8_t, decltype(&Rpixels_free)> decoded(nullptr, Rpixels_free);
        const auto size = input.tellg();
        if (size > 0 && size <= 8 * 1024 * 1024) {
            std::vector<char> bytes(static_cast<size_t>(size));
            input.seekg(0);
            if (input.read(bytes.data(), bytes.size()))
                decoded.reset(Rimage_decode(bytes.data(), bytes.size(), &w, &h));
        }
        const auto* pixels = w == width && h == height ? decoded.get() : nullptr;
        for (size_t i = 0; i < wells.size(); ++i) wells[i] = make_key_mask(kPreviewKeys[i], pixels);
        shadow = make_mask(360, 180, [](float u, float v) {
            const float d = (std::max)(0.f, rim_distance(u * width - 52, v * height - 123, 1670, 706, 54));
            return .46f * std::exp(-d * d / 1150.f) + .26f * std::exp(-d * d / 180.f);
        });
        spill = make_mask(96, 48, [](float u, float v) {
            const float x = (u - .5f) * 2, y = (v - .5f) * 2;
            const float q = clamp01(1.f - x*x - y*y);
            return q*q*q;
        });
    }
};
} // namespace keyboard_model

inline void draw_keyboard(RPainter* raw, float width, float height, float red, float green, float blue,
        float intensity, int mode, double time, bool multicolor, const keyboard_model::Model& model) {
    using keyboard_model::clamp01;
    raven::Painter p(raw);
    const float scale = (std::min)((width - 8.f) / keyboard_model::width, height / keyboard_model::height);
    if (scale <= 0) return;
    const float ox = (width - keyboard_model::width * scale) * .5f;
    const float oy = (height - keyboard_model::height * scale) * .5f;
    auto rect = [&](float x, float y, float w, float h, float rad, float r, float g, float b, float alpha) {
        p.rect(ox+x*scale, oy+y*scale, w*scale, h*scale, rad*scale, r,g,b,alpha);
    };
    auto image = [&](Rid texture, float x, float y, float w, float h, float r, float g, float b, float alpha) {
        if (texture != R_INVALID_ID)
            p.image(texture, ox+x*scale, oy+y*scale, w*scale, h*scale, r,g,b,alpha);
    };
    image(model.shadow, 0, 0, 1774, 887, .025f,.027f,.046f,1);

    // Front bevel visible from above, including the two recessed light pipes
    // seen at the front corners of this specific unit.
    for (int depth = 14; depth >= 0; --depth) {
        const float f = depth / 14.f;
        const float shade = .87f - .36f*f;
        rect(42 + 2*f,96.f + static_cast<float>(depth),1689 - 4*f,720,48,shade,shade+.012f,shade+.032f,1);
    }
    const bool photographic = model.photo.id() != R_INVALID_ID;
    if (photographic) {
        p.push_clip_rect(ox+40*scale,oy+90*scale,1693*scale,728*scale);
        image(model.photo,0,0,1774,887,.97f,.98f,1,1);
        p.pop_clip_rect();
    } else {
        rect(42,92,1689,724,48,.88f,.90f,.92f,1);
        p.circle(ox+1635*scale,oy+188*scale,60*scale,.56f,.58f,.61f,1);
        p.circle(ox+1635*scale,oy+185*scale,51*scale,.84f,.86f,.88f,1);
    }
    // The narrow frosted indicator is a physical detail of the shell.
    rect(1562,292,10,144,5,.69f,.72f,.77f,1);
    rect(1564,295,6,137,3,.90f,.92f,.94f,1);

    PreviewLight left{}, right{};
    for (size_t i = 0; i < kPreviewKeys.size(); ++i) {
        const auto& k = kPreviewKeys[i];
        const float col = (k.x + k.w*.5f - 130.f) / 98.f;
        const float row = (k.y - 134.f) / 100.f;
        auto light = preview_effect_light(mode,multicolor,red,green,blue,intensity,time,static_cast<int>(i),col,
            std::clamp(std::round(row), 0.f, 5.f));
        if (!photographic) {
            rect(k.x,k.y,k.w,k.h,7,.66f,.69f,.73f,1);
            rect(k.x+4,k.y+2,k.w-8,k.h-14,7,.90f,.92f,.95f,1);
            p.text(R_INVALID_ID, 20*scale,ox+(k.x+11)*scale,oy+(k.y+16)*scale,k.legend,.10f,.13f,.17f,1);
        }
        const float peak = (std::max)({light.r,light.g,light.b});
        if (peak <= 0.f || light.level <= 0.f) continue;
        const float strength = clamp01(light.level * peak);
        light = {light.r/peak,light.g/peak,light.b/peak,strength};
        if (i == kPreviewKeys.size()-10) left = light; // left Ctrl
        if (i == kPreviewKeys.size()-1) right = light;
        constexpr float m = keyboard_model::margin;
        image(model.wells[i],k.x-m,k.y-m,k.w+2*m,k.h+2*m, light.r,light.g,light.b,strength);
        // A small exposed portion of the south-facing emitter has a pale
        // centre. Its footprint stays the size of a switch on wide keys.
        const float emitter = (std::min)(k.w * .68f, 76.f);
        const float cx = k.x + k.w * .52f;
        image(model.spill,cx-emitter*.5f,k.y+k.h-4,emitter,8,
            .68f+.32f*light.r,.68f+.32f*light.g,.68f+.32f*light.b,.88f*strength);
    }
    const auto pipe = [&](float x, const PreviewLight& l) {
        rect(x,817,135,4,2,.70f,.72f,.75f,1);
        if (l.level <= 0.f) return;
        image(model.spill,x-50,805,235,50,l.r,l.g,l.b,.24f*l.level);
        rect(x,817,135,4,2,.15f+.85f*l.r,.15f+.85f*l.g,.15f+.85f*l.b,l.level);
    };
    pipe(102,left); pipe(1535,right);
}
