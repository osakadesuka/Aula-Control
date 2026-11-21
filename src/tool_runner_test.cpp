#include "tool_runner.hpp"
#include <cstdlib>
#include <iostream>
#include <thread>

void require(bool ok, const char* reason) {
    if (!ok) { std::cerr << reason << '\n'; std::exit(1); }
}
int main(int argc, char** argv) {
    if (argc == 2) {
        const std::string fixture = argv[1];
        if (fixture == "--echo") { std::cout << "captured output\n"; std::cerr << "captured error\n"; return 0; }
        if (fixture == "--error") { std::cout << "failed\n"; return 7; }
        if (fixture == "--flood") { std::cout << std::string(100000, 'x'); return 0; }
        if (fixture == "--hang") { Sleep(10000); return 0; }
        return 2;
    }
    wchar_t path[32768]{}; GetModuleFileNameW(nullptr, path, 32768);
    auto cancel = std::make_shared<std::atomic_bool>(false);
    auto result = run_tool(path, L"--echo", cancel);
    require(result.ok && result.output.find("captured output") != std::string::npos && result.output.find("captured error") != std::string::npos, "Capture stdout and stderr");
    require(!run_tool(path, L"--error", cancel).ok, "Reject nonzero exit");
    require(!run_tool(std::wstring(path) + L".missing", L"", cancel).ok, "Missing executable fails without waiting");
    result = run_tool(path, L"--flood", cancel);
    require(!result.ok && result.truncated && result.output.size() == 65536, "Output is bounded and drained");
    const auto began = std::chrono::steady_clock::now();
    result = run_tool(path, L"--hang", cancel, std::chrono::milliseconds(150));
    require(result.timedOut && !result.ok, "Timeout interrupts hung helper");
    require(std::chrono::steady_clock::now() - began < std::chrono::seconds(3), "Timeout returns promptly");
    std::thread stopper([&] { Sleep(150); cancel->store(true); });
    result = run_tool(path, L"--hang", cancel); stopper.join();
    require(result.cancelled && !result.ok, "Closing cancels running helper");
    result = run_tool(path, L"--echo", cancel);
    require(result.cancelled && result.output.empty(), "Already cancelled operation does not launch");
    std::cout << "Helper capture, output cap, failure, timeout and cancellation OK\n";
}
