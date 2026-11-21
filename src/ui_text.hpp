#pragma once
#include <raven.hpp>
#include "localization.hpp"
#include <functional>
#include <unordered_map>
#include <vector>

class UiText {
public:
    explicit UiText(i18n::Language language) : language_(language) {}
    std::string translate(const std::string& source) const { return i18n::translate(source, language_); }
    template<class Widget> void set(Widget widget, const std::string& source) {
        bind(widget, 0, source, [widget](const char* text) { widget.set_text(text); });
    }
    template<class Widget> void tooltip(Widget widget, const std::string& source) {
        bind(widget, 1, source, [widget](const char* text) { widget.set_tooltip(text); });
    }
    template<class Widget> void accessible(Widget widget, const std::string& source) {
        bind(widget, 2, source, [widget](const char* text) { widget.set_accessible_name(text); });
    }
    void placeholder(raven::TextInput widget, const std::string& source) {
        bind(widget, 3, source, [widget](const char* text) { widget.set_placeholder(text); });
    }
    raven::Label label(const char* source) { auto value = raven::Label::create(""); set(value, source); return value; }
    raven::Button button(const char* source) { auto value = raven::Button::create(""); set(value, source); return value; }
    raven::Checkbox checkbox(const char* source) { auto value = raven::Checkbox::create(""); set(value, source); return value; }
    void add_item(raven::ComboBox widget, const std::string& source) {
        const auto key = static_cast<unsigned long long>(widget.id());
        auto& combo = combos_[key]; combo.widget = widget; combo.sources.push_back(source);
        widget.add_item(translate(source).c_str());
    }
    std::string source(raven::Widget widget) const {
        const auto found = bindings_.find(key(widget, 0)); return found == bindings_.end() ? std::string{} : found->second.source;
    }
    std::vector<std::string> missing_translations() const {
        std::vector<std::string> missing;
        for (const auto& pair : bindings_) if (!i18n::covered(pair.second.source)) missing.push_back(pair.second.source);
        for (const auto& pair : combos_) for (const auto& source : pair.second.sources)
            if (!i18n::covered(source)) missing.push_back(source);
        return missing;
    }
    void set_language(i18n::Language language) {
        language_ = language;
        for (const auto& pair : bindings_) pair.second.apply(translate(pair.second.source).c_str());
        for (const auto& pair : combos_) {
            const auto& combo = pair.second; const auto selected = combo.widget.get_selected();
            combo.widget.clear_items();
            for (const auto& source : combo.sources) combo.widget.add_item(translate(source).c_str());
            combo.widget.set_selected(selected);
        }
        Rlocale_set_language(i18n::code(language));
    }
private:
    struct Binding { std::string source; std::function<void(const char*)> apply; };
    struct Combo { raven::ComboBox widget; std::vector<std::string> sources; };
    static unsigned long long key(raven::Widget widget, unsigned property) { return (static_cast<unsigned long long>(widget.id()) << 3) | property; }
    template<class Widget> void bind(Widget widget, unsigned property, const std::string& source, std::function<void(const char*)> apply) {
        const auto id = key(widget, property);
        const auto previous = bindings_.find(id);
        if (previous != bindings_.end() && previous->second.source == source) return;
        apply(translate(source).c_str()); bindings_[id] = {source, std::move(apply)};
    }
    i18n::Language language_;
    std::unordered_map<unsigned long long, Binding> bindings_;
    std::unordered_map<unsigned long long, Combo> combos_;
};
