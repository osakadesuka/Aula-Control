#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include "localization.hpp"

// Keep a real Windows menu: Windows owns selection, Escape, arrows and hit tests.
// Only its painting changes; screen readers and high contrast keep native text.
class TrayMenu {
public:
    struct Entry { UINT command; const wchar_t* text; bool separator; };
    inline static constexpr std::array<Entry, 4> entries{{
        {1, L"&Abrir AULA Control", false}, {2, L"&Preferencias", false},
        {0, L"", true}, {3, L"&Salir de AULA Control", false}}};
    inline static constexpr std::array<Entry, 4> englishEntries{{
        {1, L"&Open AULA Control", false}, {2, L"&Preferences", false},
        {0, L"", true}, {3, L"E&xit AULA Control", false}}};
    static bool custom_paint_allowed() {
        HIGHCONTRASTW contrast{sizeof(HIGHCONTRASTW)}; BOOL reader = FALSE;
        if (!SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0)
            || !SystemParametersInfoW(SPI_GETSCREENREADER, 0, &reader, 0)) return false;
        return !(contrast.dwFlags & HCF_HIGHCONTRASTON) && !reader;
    }
    explicit TrayMenu(UINT dpi, bool custom = custom_paint_allowed(), i18n::Language language = i18n::Language::spanish)
        : dpi_(dpi ? dpi : 96), custom_(custom), language_(language) {
        handle_ = CreatePopupMenu();
        if (!handle_) return;
        if (custom_) {
            NONCLIENTMETRICSW metrics{sizeof(NONCLIENTMETRICSW)};
            if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi_)) {
                font_ = CreateFontIndirectW(&metrics.lfMenuFont);
                metrics.lfMenuFont.lfWeight = FW_SEMIBOLD; boldFont_ = CreateFontIndirectW(&metrics.lfMenuFont);
            }
            background_ = CreateSolidBrush(RGB(27, 32, 40)); hover_ = CreateSolidBrush(RGB(43, 60, 75));
            if (!font_ || !boldFont_ || !background_ || !hover_) custom_ = false;
        }
        if (custom_) {
            MENUINFO info{sizeof(MENUINFO)}; info.fMask = MIM_BACKGROUND; info.hbrBack = background_;
            SetMenuInfo(handle_, &info);
        }
        for (UINT i = 0; i < items().size(); ++i) {
            const auto& entry = items()[i];
            MENUITEMINFOW info{sizeof(MENUITEMINFOW)};
            info.fMask = MIIM_FTYPE | MIIM_ID | MIIM_DATA | MIIM_STRING;
            info.fType = (entry.separator ? MFT_SEPARATOR : MFT_STRING) | (custom_ ? MFT_OWNERDRAW : 0);
            info.wID = entry.command; info.dwItemData = reinterpret_cast<ULONG_PTR>(&entry);
            info.dwTypeData = const_cast<wchar_t*>(entry.text);
            if (!InsertMenuItemW(handle_, i, TRUE, &info)) { DestroyMenu(handle_); handle_ = nullptr; break; }
        }
        if (handle_) SetMenuDefaultItem(handle_, 1, FALSE);
    }
    ~TrayMenu() {
        if (handle_) DestroyMenu(handle_);
        if (font_) DeleteObject(font_); if (boldFont_) DeleteObject(boldFont_);
        if (background_) DeleteObject(background_); if (hover_) DeleteObject(hover_);
    }
    HMENU handle() const { return handle_; }
    bool custom() const { return custom_; }
    const std::array<Entry, 4>& items() const { return language_ == i18n::Language::english ? englishEntries : entries; }
    static int mnemonic_index(wchar_t key, i18n::Language language = i18n::Language::spanish) {
        if (language == i18n::Language::english) {
            switch (std::towlower(key)) { case L'o': return 0; case L'p': return 1; case L'x': return 3; default: return -1; }
        }
        switch (std::towlower(key)) { case L'a': return 0; case L'p': return 1; case L's': return 3; default: return -1; }
    }
    bool measure(MEASUREITEMSTRUCT& item) const {
        if (!custom_ || item.CtlType != ODT_MENU) return false;
        const auto entry = checked_entry(item.itemData); if (!entry) return false;
        if (entry->separator) { item.itemWidth = scale(220); item.itemHeight = scale(9); return true; }
        const auto dc = GetDC(nullptr); if (!dc) return false;
        const auto previous = SelectObject(dc, entry->command == 1 ? boldFont_ : font_);
        SIZE text{}; GetTextExtentPoint32W(dc, entry->text, static_cast<int>(wcslen(entry->text)), &text);
        SelectObject(dc, previous); ReleaseDC(nullptr, dc);
        item.itemWidth = static_cast<UINT>((std::max)(scale(220), static_cast<int>(text.cx) + scale(36)));
        item.itemHeight = static_cast<UINT>((std::max)(scale(34), static_cast<int>(text.cy) + scale(14))); return true;
    }
    bool draw(const DRAWITEMSTRUCT& item) const {
        if (!custom_ || item.CtlType != ODT_MENU) return false;
        const auto entry = checked_entry(item.itemData); if (!entry) return false;
        const int saved = SaveDC(item.hDC); FillRect(item.hDC, &item.rcItem, background_);
        if (entry->separator) {
            const auto pen = CreatePen(PS_SOLID, 1, RGB(59, 70, 84)); const auto old = SelectObject(item.hDC, pen);
            const auto y = (item.rcItem.top + item.rcItem.bottom) / 2;
            MoveToEx(item.hDC, item.rcItem.left + scale(12), y, nullptr); LineTo(item.hDC, item.rcItem.right - scale(12), y);
            SelectObject(item.hDC, old); DeleteObject(pen);
        } else {
            const bool selected = (item.itemState & ODS_SELECTED) != 0;
            if (selected) {
                SelectObject(item.hDC, GetStockObject(NULL_PEN)); SelectObject(item.hDC, hover_);
                RoundRect(item.hDC, item.rcItem.left + scale(3), item.rcItem.top + scale(1), item.rcItem.right - scale(3), item.rcItem.bottom - scale(1), scale(8), scale(8));
            }
            auto text = item.rcItem; text.left += scale(14); text.right -= scale(14);
            SelectObject(item.hDC, entry->command == 1 ? boldFont_ : font_); SetBkMode(item.hDC, TRANSPARENT);
            SetTextColor(item.hDC, selected ? RGB(87, 215, 236) : RGB(225, 233, 241));
            DrawTextW(item.hDC, entry->text, -1, &text, DT_SINGLELINE | DT_VCENTER | ((item.itemState & ODS_NOACCEL) ? DT_HIDEPREFIX : 0));
        }
        RestoreDC(item.hDC, saved); return true;
    }
private:
    int scale(int value) const { return MulDiv(value, dpi_, 96); }
    const Entry* checked_entry(ULONG_PTR pointer) const {
        for (const auto& entry : items()) if (pointer == reinterpret_cast<ULONG_PTR>(&entry)) return &entry;
        return nullptr;
    }
    HMENU handle_ = nullptr;
    HFONT font_ = nullptr, boldFont_ = nullptr;
    HBRUSH background_ = nullptr, hover_ = nullptr;
    UINT dpi_;
    bool custom_;
    i18n::Language language_;
};
