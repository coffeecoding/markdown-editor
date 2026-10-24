# Win32 app demo

The native Markdown editor is in **[App Demo](App%20Demo/README.md)**, including
source, build/smoke scripts, and an x64 release executable. The original starter
below is preserved.

A minimal native Win32 C++ application built for x64 with MSVC, CMake, and Ninja. The window has a button that toggles its light/dark theme and a label showing the active theme.

## Toolchain

Install these prerequisites if they are missing:

- CMake: `winget install Kitware.CMake`
- Ninja: `winget install Ninja-build.Ninja`
- Visual Studio Build Tools (or Visual Studio): use the Visual Studio Installer and select **Desktop development with C++**, including MSVC and Windows SDK **10.0.26100.0**.

## Build and run

Open an **x64 Native Tools Command Prompt for VS**, then run:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_VERSION=10.0.26100.0
cmake --build build
build\win32_app_demo.exe
```

From a normal shell, first run Visual Studio's `VsDevCmd.bat` with `-arch=x64 -host_arch=x64 -winsdk=10.0.26100.0`, then use the same CMake commands.
