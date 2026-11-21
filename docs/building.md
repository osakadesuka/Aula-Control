# Building AULA Control

[English README](../README.md) · [Notas de desarrollo en español](desarrollo.md)

Run these commands from the repository root. Building requires Windows x64, Git, CMake 3.20 or newer, and Visual Studio or Build Tools with the Desktop development with C++ workload and a Windows SDK. Packaging uses PowerShell 7. CMake downloads HIDAPI 0.15.0 and Vulkan headers when needed. Raven 0.2.3 is vendored in `third_party/`.

## Build and test

```powershell
cmake -S . -B build -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Start `build/Release/aula_control.exe` for the graphical app. Keep `assets/`, `rgb_prueba.exe`, `bateria_f75.exe` and `hidapi.dll` beside it. Developer builds may require an installed Visual C++ runtime; Setup includes app-local runtime DLLs. A real graphical session needs Vulkan support.

The current 13 CTests cover protocol construction, device selection/state, battery replies, media gestures, profiles, last-lighting memory, helper execution, preview cadence/effects, desktop integration and GUI layout. The GUI test uses the NULL backend and isolated stores; it does not send HID settings, enable media remapping or modify real profiles. Automated tests do not establish compatibility with an untested keyboard variant.

## Create Setup

```powershell
.\packaging\build-setup.ps1
```

This builds/tests Release in `build/release-1.0/`, stages an explicit runtime file list, checks the staged executables and compiles Setup. Outputs in `dist/` are the unsigned installer, SHA256, manifest and bilingual release notes. Diagnostic tools, tests, personal logs and backups are excluded from Setup.

`VERSION` is the version source. CMake embeds it in the app metadata. Use a new version for a different release; keep existing release assets immutable.

| Parameter | Purpose |
| --- | --- |
| `-BuildDirectory <path>` | Choose a separate CMake build directory. |
| `-IsccPath <ISCC.exe>` | Use an existing Inno Setup compiler. |
| `-RuntimeDirectory <path>` | Supply the x64 Microsoft Visual C++ redist CRT directory. |
| `-SkipBuild` | Reuse a matching Release build and recheck its staged package. |

Without `-IsccPath`, the script obtains the official pinned Inno Setup 7.1.1 installer, verifies its published SHA256 and publisher signature, and extracts its portable compiler under `build/tools/`. The compiler is not registered as an installed application.

## Installer round-trip test

```powershell
.\packaging\test-setup.ps1
```

Use a disposable Windows user or runner. This test requires AULA to be closed and refuses to run if a normal AULA installer registration exists. It temporarily installs under `build/installer-smoke/`, checks manifest hashes and the Start-menu shortcut, upgrades in the other language and uninstalls twice to check startup handling. It verifies that user data and an untracked file are retained, and restores the original AULA startup value in `finally`. Do not run it over a daily-use installation.

## Continuous integration

`.github/workflows/windows.yml` uses a standard Windows runner to build, test, package and perform the installer round trip. It uploads artifacts and diagnostic logs on failure. Its repository token is read-only; it never publishes releases or changes visibility. Relevant pushes, pull requests and manual dispatch can run it. Private-repository runs consume the account's available Actions allowance. There is no scheduled workflow.

The README status image is GitHub's actual workflow badge. Platform and version badges describe the target and `VERSION`. A license badge will be added only after a project-wide license is selected.

Detailed historical protocol research is retained in Spanish under `docs/`, starting with [desarrollo.md](desarrollo.md). It distinguishes receiver tests, USB observations and unsupported modes. Do not generalize packets to another keyboard based on appearance or VID/PID alone.
