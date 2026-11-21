#pragma once
#include <raven.hpp>
#include "ui_skin.hpp"
#include <memory>
#include <cmath>

namespace ui {
enum class SurfaceRole { panel, hero, compact, sidebar };
inline raven::Panel box(RWidgetType type, float gap) {
    auto value = raven::Panel(raven::Widget::create(type)); value.set_spacing(gap); return value;
}

// Paired shadows share one geometry; the content remains a normal Raven widget.
class Surface : public raven::Widget {
public:
    static Surface create(float padding, SurfaceRole role = SurfaceRole::panel) {
        auto light = raven::Border::create();
        auto dark = raven::Border::create();
        auto face = raven::Border::create();
        for (const auto& layer : {light, dark, face}) {
            layer.set_background(0.15f, 0.168f, 0.19f, 1);
            layer.set_corner_radius(22, 22, 22, 22);
        }
        light.set_color(R_COLOR_SHADOW, 0.44f, 0.49f, 0.57f, 0.10f);
        light.set_shadow(16, -5, -5);
        dark.set_color(R_COLOR_SHADOW, 0.015f, 0.021f, 0.03f, 0.62f);
        dark.set_shadow(22, 7, 9);
        face.set_outline(1, 0.40f, 0.45f, 0.52f, 0.22f);
        face.set_padding(padding, padding, padding, padding);
        light.add_child(dark); dark.add_child(face);
        return Surface(light, dark, face, role);
    }
    void add_child(Rid child) const { content_.add_child(child); }
    void set_skin(Skin skin) const {
        const bool glass = skin == Skin::glass;
        const bool hero = role_ == SurfaceRole::hero, compact = role_ == SurfaceRole::compact;
        const bool sidebar = role_ == SurfaceRole::sidebar;
        for (const auto& layer : {light_, dark_}) {
            layer.set_material(R_MATERIAL_SOLID);
            layer.set_background(.15f, .168f, .19f, glass ? 0.0f : 1.0f);
        }
        light_.set_color(R_COLOR_SHADOW, .44f, .51f, .59f, compact ? .055f : .085f);
        dark_.set_color(R_COLOR_SHADOW, .012f, .018f, .028f, compact ? .34f : .48f);
        light_.set_shadow(glass ? 0.0f : compact ? 9.0f : 14.0f, -4, -4);
        dark_.set_shadow(glass ? 0.0f : compact ? 12.0f : 20.0f, 5, 7);
        content_.set_material(glass ? R_MATERIAL_FROSTED_GLASS : R_MATERIAL_SOLID);
        content_.set_background(glass ? .055f : sidebar ? .125f : .145f,
            glass ? .08f : sidebar ? .145f : .165f, glass ? .125f : sidebar ? .175f : .20f,
            glass ? (sidebar ? .62f : hero ? .38f : compact ? .42f : .48f) : 1);
        content_.set_outline(1, hero ? .35f : glass ? .65f : .40f,
            hero ? .66f : glass ? .78f : .46f, hero ? .77f : glass ? .90f : .54f,
            hero ? .24f : compact ? .10f : .14f);
        content_.set_shadow(glass ? (compact ? 12.0f : 22.0f) : 0.0f, 0, compact ? 4.0f : 7.0f);
        content_.set_color(R_COLOR_SHADOW, .005f, .012f, .025f, compact ? .22f : .30f);
    }
private:
    Surface(raven::Border light, raven::Border dark, raven::Border content, SurfaceRole role)
        : raven::Widget(light), light_(light), dark_(dark), content_(content), role_(role) {}
    raven::Border light_, dark_, content_;
    SurfaceRole role_;
};

struct NavigationItem {
    raven::Button button;
    raven::Label text;
    raven::Custom icon;
    std::shared_ptr<bool> selected;
    std::shared_ptr<Skin> skin;
    void select(bool value) const {
        *selected = value;
        const bool glass = *skin == Skin::glass;
        button.set_material(glass && value ? R_MATERIAL_FROSTED_GLASS : R_MATERIAL_SOLID);
        button.set_color(R_COLOR_CONTROL, value ? .12f : .145f, value ? .22f : .165f, value ? .27f : .20f, glass ? (value ? .44f : 0.0f) : value ? 1.0f : 0.0f);
        button.set_color(R_COLOR_CONTROL_HOVERED, glass ? .35f : .19f, glass ? .55f : .22f, glass ? .65f : .25f, glass ? .23f : 1);
        button.set_color(R_COLOR_CONTROL_PRESSED, .105f, .14f, .17f, glass ? .55f : 1);
        button.set_color(R_COLOR_CONTROL_BORDER, .28f, .72f, .80f, value ? .25f : 0.0f);
        text.set_color(value ? .53f : .68f, value ? .90f : .75f, value ? .96f : .83f, 1);
        icon.invalidate();
    }
    void set_skin(Skin value) const { *skin = value; select(*selected); }
};
inline NavigationItem navigation(const char* title, int type) {
    NavigationItem item{raven::Button::create(""), raven::Label::create(title), raven::Custom::create(), std::make_shared<bool>(false), std::make_shared<Skin>(Skin::neumorphic)};
    item.button.set_size(-1, 48);
    item.button.set_metric(R_METRIC_CORNER_RADIUS, 14);
    item.button.set_accessible_name(title); item.button.set_tooltip(title);
    item.button.set_metric(R_METRIC_PADDING_X, 12); item.button.set_metric(R_METRIC_PADDING_Y, 10);
    item.button.set_color(R_COLOR_CONTROL_HOVERED, 0.19f, 0.22f, 0.25f, 1);
    item.button.set_color(R_COLOR_CONTROL_PRESSED, 0.105f, 0.14f, 0.17f, 1);
    item.text.set_text_size(14); item.text.slot_set_valign(R_VALIGN_CENTER);
    item.icon.set_size(22, 22); item.icon.slot_set_valign(R_VALIGN_CENTER); item.icon.set_accessible_hidden(R_TRUE);
    item.icon.set_paint_callback([selected = item.selected, type, spring = RSpring{}](Rid, RPainter* raw, float, float) mutable {
        raven::Painter p(raw);
        const float active = p.spring(&spring, *selected ? 1.0f : 0.0f, R_MOTION_SMOOTH);
        const float r = 0.62f - active * 0.27f, g = 0.69f + active * 0.16f, b = 0.77f + active * 0.15f;
        if (type == 0) {
            p.rect_outline(1, 4, 20, 14, 3, 1.4f, r, g, b, 1);
            for (int row = 0; row < 2; ++row) for (int col = 0; col < 5; ++col)
                p.rect(4 + col * 3.2f, 7 + row * 3.2f, 1.6f, 1.6f, 0.5f, r, g, b, 1);
            p.line(7, 15, 15, 15, 1.4f, r, g, b, 1);
        } else if (type == 1) {
            p.circle(11, 11, 9, r, g, b, 0.12f);
            for (int i = 0; i < 32; ++i) {
                const float a = i * 6.2831853f / 32, c = (i + 1) * 6.2831853f / 32;
                p.line(11 + 8 * std::cos(a), 11 + 8 * std::sin(a), 11 + 8 * std::cos(c), 11 + 8 * std::sin(c), 1.4f, r, g, b, 1);
            }
            p.line(11, 5, 11, 10, 1.8f, r, g, b, 1);
        } else if (type == 2) {
            p.rect_outline(6, 2, 14, 14, 3, 1.4f, r, g, b, 0.5f);
            p.rect_outline(2, 6, 14, 14, 3, 1.4f, r, g, b, 1);
            p.line(6, 11, 12, 11, 1.4f, r, g, b, 1); p.line(6, 15, 10, 15, 1.4f, r, g, b, 1);
        } else if (type == 3) {
            p.rect_outline(2, 6, 17, 11, 2, 1.4f, r, g, b, 1);
            p.rect(5, 9, 9, 5, 1, r, g, b, 0.65f); p.line(21, 9, 21, 14, 1.4f, r, g, b, 1);
        } else if (type == 4) {
            p.line(2, 10, 11, 3, 1.4f, r, g, b, 1); p.line(11, 3, 20, 10, 1.4f, r, g, b, 1);
            p.line(5, 9, 5, 19, 1.4f, r, g, b, 1); p.line(17, 9, 17, 19, 1.4f, r, g, b, 1);
            p.line(5, 19, 17, 19, 1.4f, r, g, b, 1); p.rect_outline(9, 13, 4, 6, 1, 1.2f, r, g, b, 1);
        } else if (type == 5) {
            p.line(11, 2, 13.7f, 8.3f, 1.4f, r, g, b, 1); p.line(13.7f, 8.3f, 20, 11, 1.4f, r, g, b, 1);
            p.line(20, 11, 13.7f, 13.7f, 1.4f, r, g, b, 1); p.line(13.7f, 13.7f, 11, 20, 1.4f, r, g, b, 1);
            p.line(11, 20, 8.3f, 13.7f, 1.4f, r, g, b, 1); p.line(8.3f, 13.7f, 2, 11, 1.4f, r, g, b, 1);
            p.line(2, 11, 8.3f, 8.3f, 1.4f, r, g, b, 1); p.line(8.3f, 8.3f, 11, 2, 1.4f, r, g, b, 1);
        } else {
            p.rect_outline(4, 4, 14, 14, 4, 1.4f, r, g, b, 1);
            p.circle(11, 11, 3, r, g, b, .5f);
            for (int i = 0; i < 4; ++i) {
                const float a = i * 1.5707963f;
                p.line(11 + 8 * std::cos(a), 11 + 8 * std::sin(a), 11 + 10 * std::cos(a), 11 + 10 * std::sin(a), 2, r, g, b, 1);
            }
        }
    });
    auto row = box(R_WIDGET_HBOX, 12); row.add_child(item.icon); row.add_child(item.text); item.button.add_child(row); item.select(false);
    return item;
}
} // namespace ui
