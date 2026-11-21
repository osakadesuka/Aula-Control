#pragma once
#include <windows.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>

struct ToolResult {
    bool ok = false;
    std::string output;
    bool timedOut = false, cancelled = false, truncated = false;
};
inline ToolResult run_tool(const std::wstring& exe, const std::wstring& args,
                           const std::shared_ptr<std::atomic_bool>& cancel,
                           std::chrono::milliseconds timeout = std::chrono::seconds(8)) {
    ToolResult result;
    if (cancel && cancel->load()) { result.cancelled = true; return result; }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return result;
    if (!SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0)) { CloseHandle(readPipe); CloseHandle(writePipe); return result; }
    STARTUPINFOW si{}; si.cb = sizeof(si); si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; si.hStdOutput = writePipe; si.hStdError = writePipe; si.hStdInput = nullptr;
    PROCESS_INFORMATION pi{};
    std::wstring command = L"\"" + exe + L"\" " + args;
    const BOOL started = CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writePipe);
    if (!started) { CloseHandle(readPipe); return result; }
    CloseHandle(pi.hThread);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    constexpr std::size_t limit = 65536;
    bool ioError = false;
    DWORD exitCode = 1;
    while (true) {
        result.cancelled = cancel && cancel->load();
        result.timedOut = !result.cancelled && std::chrono::steady_clock::now() >= deadline;
        if (result.cancelled || result.timedOut) {
            // This handle belongs solely to the helper we created for this operation.
            TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 1000); break;
        }
        DWORD available = 0;
        if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr)) {
            if (GetLastError() != ERROR_BROKEN_PIPE) ioError = true;
        }
        if (available) {
            char buffer[4096]; DWORD count = 0;
            if (!ReadFile(readPipe, buffer, (std::min)(available, static_cast<DWORD>(sizeof(buffer))), &count, nullptr)) ioError = true;
            else {
                const auto kept = (std::min)(static_cast<std::size_t>(count), limit - result.output.size());
                result.output.append(buffer, kept); result.truncated = result.truncated || kept != count;
            }
            continue; // Drain available bytes; check cancellation and deadline between chunks.
        }
        const DWORD status = WaitForSingleObject(pi.hProcess, 25);
        if (status == WAIT_OBJECT_0) {
            // The process may have written its last bytes between Peek and Wait.
            if (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available) continue;
            GetExitCodeProcess(pi.hProcess, &exitCode); break;
        }
        if (status == WAIT_FAILED) {
            ioError = true; TerminateProcess(pi.hProcess, 1); WaitForSingleObject(pi.hProcess, 1000); break;
        }
    }
    CloseHandle(readPipe); CloseHandle(pi.hProcess);
    result.ok = exitCode == 0 && !result.cancelled && !result.timedOut && !result.truncated && !ioError;
    return result;
}
