#pragma once
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include "general_preferences.hpp"
#include "tray_menu.hpp"
#include <string>
#include <optional>
#include <utility>

// The mutex lives for the whole process, including while its window is hidden.
// An auto-reset event also handles a second launch during initial UI creation.
class SingleInstance {
public:
    enum class Result { owner, existing, error };
    Result acquire(bool requestRestore, const std::wstring& name = L"Local\\AulaControl.Desktop.1") {
        restore_ = CreateEventW(nullptr, FALSE, FALSE, (name + L".Restore").c_str());
        if (!restore_) return Result::error;
        mutex_ = CreateMutexW(nullptr, FALSE, name.c_str());
        if (!mutex_) return Result::error;
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (requestRestore && !SetEvent(restore_)) return Result::error;
            return Result::existing;
        }
        return Result::owner;
    }
    bool restore_requested() const { return restore_ && WaitForSingleObject(restore_, 0) == WAIT_OBJECT_0; }
    ~SingleInstance() { if (mutex_) CloseHandle(mutex_); if (restore_) CloseHandle(restore_); }
private:
    HANDLE mutex_ = nullptr, restore_ = nullptr;
};

inline std::wstring startup_command(const std::wstring& executable) {
    if (executable.empty() || executable.find_first_of(L"\"\r\n") != std::wstring::npos) return {};
    const auto command = L"\"" + executable + L"\" --background";
    return command.size() <= 260 ? command : std::wstring{};
}
class StartupRegistration {
public:
    explicit StartupRegistration(std::wstring executable,
        std::wstring key = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run")
        : command_(startup_command(executable)), key_(std::move(key)) {}
    std::optional<bool> enabled() const {
        HKEY key = nullptr;
        const auto opened = RegOpenKeyExW(HKEY_CURRENT_USER, key_.c_str(), 0, KEY_QUERY_VALUE, &key);
        if (opened == ERROR_FILE_NOT_FOUND) return false;
        if (opened != ERROR_SUCCESS) return {};
        DWORD type = 0, size = 0;
        auto status = RegQueryValueExW(key, L"AULA Control", nullptr, &type, nullptr, &size);
        if (status == ERROR_FILE_NOT_FOUND) { RegCloseKey(key); return false; }
        if (status != ERROR_SUCCESS || type != REG_SZ || size < sizeof(wchar_t) || size > 4096 || size % sizeof(wchar_t)) {
            RegCloseKey(key); return {};
        }
        std::wstring data(size / sizeof(wchar_t), L'\0');
        status = RegQueryValueExW(key, L"AULA Control", nullptr, &type, reinterpret_cast<BYTE*>(data.data()), &size);
        RegCloseKey(key);
        if (status != ERROR_SUCCESS || data.back() != L'\0') return {};
        data.pop_back();
        return !command_.empty() && data == command_;
    }
    bool set(bool enabled) const {
        HKEY key = nullptr;
        if (enabled) {
            if (command_.empty() || RegCreateKeyExW(HKEY_CURRENT_USER, key_.c_str(), 0, nullptr, 0,
                KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
            const auto status = RegSetValueExW(key, L"AULA Control", 0, REG_SZ,
                reinterpret_cast<const BYTE*>(command_.c_str()), static_cast<DWORD>((command_.size() + 1) * sizeof(wchar_t)));
            RegCloseKey(key); return status == ERROR_SUCCESS;
        }
        const auto opened = RegOpenKeyExW(HKEY_CURRENT_USER, key_.c_str(), 0, KEY_SET_VALUE, &key);
        if (opened == ERROR_FILE_NOT_FOUND) return true;
        if (opened != ERROR_SUCCESS) return false;
        const auto status = RegDeleteValueW(key, L"AULA Control");
        RegCloseKey(key); return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
private:
    std::wstring command_, key_;
};

inline std::optional<WindowBounds> capture_window_bounds(HWND window) {
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
    if (!window || !GetWindowPlacement(window, &placement)) return {};
    const auto& rect = placement.rcNormalPosition;
    WindowBounds value{rect.left, rect.top, rect.right, rect.bottom,
        placement.showCmd == SW_SHOWMAXIMIZED || (placement.flags & WPF_RESTORETOMAXIMIZED) != 0};
    return value.valid() ? std::optional<WindowBounds>(value) : std::optional<WindowBounds>{};
}
inline bool restore_window_bounds(HWND window, const WindowBounds& value) {
    if (!window || !value.valid()) return false;
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
    placement.showCmd = SW_HIDE;
    placement.rcNormalPosition = {value.left, value.top, value.right, value.bottom};
    // SetWindowPlacement repairs positions left off-screen by a removed monitor.
    return SetWindowPlacement(window, &placement) != FALSE;
}

class DesktopTray {
public:
    enum class Action { none, show, preferences, exit };
    void set_language(i18n::Language language) { language_ = language; }
    bool create() {
        WNDCLASSW type{}; type.lpfnWndProc = procedure; type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = className;
        if (!RegisterClassW(&type) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
        taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
        icon_ = static_cast<HICON>(LoadImageW(type.hInstance, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, 0));
        if (!icon_) return false;
        // Top-level hidden window receives TaskbarCreated; HWND_MESSAGE does not.
        window_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_LAYERED, className, L"AULA Control background", WS_POPUP,
            0, 0, 1, 1, nullptr, nullptr, type.hInstance, this);
        if (window_) SetLayeredWindowAttributes(window_, 0, 0, LWA_ALPHA);
        return window_ && add();
    }
    bool available() const { return added_; }
    bool retry() { return added_ || (window_ && icon_ && add()); }
    Action take_action() { return std::exchange(action_, Action::none); }
    bool ending_session() const { return endingSession_; }
    // Also used by the isolated interactive regression test; no tray command is injected.
    void show_context_menu(POINT point) { menu(point); }
    void update(const std::string& status) {
        const auto text = std::string("AULA Control · ") + status;
        wchar_t wide[128]{};
        if (!MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide, 128)) return;
        if (tip_ == wide) return;
        tip_ = wide;
        if (added_) {
            auto data = identity(); data.uFlags = NIF_TIP | NIF_SHOWTIP;
            wcsncpy_s(data.szTip, tip_.c_str(), _TRUNCATE);
            if (!Shell_NotifyIconW(NIM_MODIFY, &data)) added_ = false;
        }
    }
    ~DesktopTray() {
        if (window_) { auto data = identity(); Shell_NotifyIconW(NIM_DELETE, &data); DestroyWindow(window_); }
        if (icon_) DestroyIcon(icon_);
    }
private:
    inline static constexpr wchar_t className[] = L"AulaControl.Tray.1";
    static constexpr UINT callbackMessage = WM_APP + 57;
    HWND window_ = nullptr;
    HICON icon_ = nullptr;
    UINT taskbarCreated_ = 0;
    bool added_ = false, endingSession_ = false, version4_ = false;
    Action action_ = Action::none;
    TrayMenu* activeMenu_ = nullptr;
    i18n::Language language_ = i18n::Language::spanish;
    std::wstring tip_ = L"AULA Control · Comprobando teclado...";
    NOTIFYICONDATAW identity() const {
        NOTIFYICONDATAW data{}; data.cbSize = sizeof(data); data.hWnd = window_; data.uID = 1; return data;
    }
    bool add() {
        auto data = identity(); data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        data.uCallbackMessage = callbackMessage; data.hIcon = icon_;
        wcsncpy_s(data.szTip, tip_.c_str(), _TRUNCATE);
        added_ = Shell_NotifyIconW(NIM_ADD, &data) != FALSE;
        if (added_) { data.uVersion = NOTIFYICON_VERSION_4; version4_ = Shell_NotifyIconW(NIM_SETVERSION, &data) != FALSE; }
        return added_;
    }
    void menu(POINT point) {
        if (activeMenu_ || !window_) return;
        // A permanently hidden owner cannot reliably become the foreground window.
        // Show a transparent 1px tool window only for the native menu's lifetime.
        SetWindowPos(window_, nullptr, point.x, point.y, 1, 1, SWP_NOZORDER | SWP_NOACTIVATE);
        ShowWindow(window_, SW_SHOWNOACTIVATE); SetForegroundWindow(window_);
        if (GetForegroundWindow() != window_) { ShowWindow(window_, SW_HIDE); return; }
        TrayMenu appearance(GetDpiForWindow(window_), TrayMenu::custom_paint_allowed(), language_);
        if (!appearance.handle()) { ShowWindow(window_, SW_HIDE); return; }
        activeMenu_ = &appearance;
        const auto selected = TrackPopupMenuEx(appearance.handle(), TPM_RETURNCMD | TPM_RIGHTBUTTON,
            point.x, point.y, window_, nullptr);
        activeMenu_ = nullptr; ShowWindow(window_, SW_HIDE); PostMessageW(window_, WM_NULL, 0, 0);
        if (selected == 1) action_ = Action::show;
        else if (selected == 2) action_ = Action::preferences;
        else if (selected == 3) action_ = Action::exit;
        else { auto data = identity(); Shell_NotifyIconW(NIM_SETFOCUS, &data); }
    }
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        auto self = reinterpret_cast<DesktopTray*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<DesktopTray*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self)); self->window_ = window;
        }
        if (self) {
            if (message == WM_MEASUREITEM && self->activeMenu_ && lparam
                && self->activeMenu_->measure(*reinterpret_cast<MEASUREITEMSTRUCT*>(lparam))) return TRUE;
            if (message == WM_DRAWITEM && self->activeMenu_ && lparam
                && self->activeMenu_->draw(*reinterpret_cast<DRAWITEMSTRUCT*>(lparam))) return TRUE;
            if (message == WM_MENUCHAR && self->activeMenu_ && self->activeMenu_->custom()) {
                const auto index = TrayMenu::mnemonic_index(static_cast<wchar_t>(LOWORD(wparam)), self->language_);
                return MAKELRESULT(index < 0 ? 0 : index, index < 0 ? MNC_IGNORE : MNC_EXECUTE);
            }
            if (self->activeMenu_ && (message == WM_CANCELMODE || (message == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE))) EndMenu();
            if (self->taskbarCreated_ && message == self->taskbarCreated_) { self->added_ = false; self->add(); return 0; }
            if (message == callbackMessage) {
                const auto event = self->version4_ ? LOWORD(lparam) : static_cast<UINT>(lparam);
                if (event == NIN_SELECT || event == NIN_KEYSELECT || event == WM_LBUTTONUP) self->action_ = Action::show;
                else if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP) {
                    POINT point{};
                    if (self->version4_) point = {GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)};
                    else GetCursorPos(&point);
                    if (point.x == -1 && point.y == -1) GetCursorPos(&point);
                    self->menu(point);
                }
                return 0;
            }
            if (message == WM_QUERYENDSESSION) return TRUE;
            if (message == WM_ENDSESSION && wparam) self->endingSession_ = true;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    }
};
