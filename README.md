# QuakeCore

A simple and clean version of the Quake source code, maintained here as the "QuakeCore" project. This repository contains the classic Quake engine sources using OpenGL rendering via SDL2; you still need valid Quake PAK files (for example `pak0.pak` inside an `id1/` directory) to run the game data. The Shareware version works fine, or you can buy the full version on Steam.([QUAKE STEAM](https://store.steampowered.com/app/2310/Quake/)).
This README explains how to build the project on common desktop platforms (Linux, macOS, Windows) using CMake and how to run the resulting executable.

## License

See `gnu.txt` in the repository root. This source tree is distributed under the terms listed there (GNU GPL v2 as noted in the original release). You are responsible for owning a valid copy of the Quake PAK data files to use with this engine.

## Requirements

- Supported platforms: Linux, macOS, Windows (the code uses SDL2 for audio/video/input and OpenGL for rendering, with CMake as the build system).
- A C compiler / toolchain (gcc/clang on Unix-like systems, MSVC or MinGW on Windows).
- CMake (minimum 3.16, newer is fine).
- A generator: make, Ninja, or an IDE/Visual Studio on Windows.
- SDL2 development libraries (required for audio/video/input). The package name varies by platform.
- OpenGL development libraries (required for rendering).
- Valid Quake PAK files (`id1/pak0.pak` at minimum). This project does not include the PAK files — place your `id1/` directory next to the built executable or provide the path when running.

### Platform-specific dependency hints

**Debian/Ubuntu (Linux):**
```
sudo apt update
sudo apt install build-essential cmake libsdl2-dev libgl1-mesa-dev
```

**Fedora (Linux):**
```
sudo dnf install @development-tools cmake SDL2-devel mesa-libGL-devel
```

**Arch (Linux):**
```
sudo pacman -Syu base-devel cmake sdl2 mesa
```

**macOS (Homebrew):**
```
brew update
brew install cmake sdl2
```

**Windows (two common options):**
- **Visual Studio (recommended):** install "Desktop development with C++" and CMake support; install SDL2 development files or use vcpkg to install SDL2.
- **MSYS2/MinGW:** open MSYS2 MinGW 64-bit shell and install packages:
```
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake mingw-w64-x86_64-SDL2
```

## Build (out-of-source, cross-platform)

This repository uses CMake. We recommend an out-of-source build to keep generated files separate from the source tree. The examples below show common commands for each platform and generator.

**General (single-config generators like Make or Ninja):**
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j$(nproc)
```

> Note for macOS: `$(nproc)` may not be defined — use `sysctl -n hw.ncpu` or omit the parallel flag.

**For multi-config generators (Visual Studio on Windows)** specify the configuration when building:
```
# Example for Visual Studio (run in PowerShell or Developer Command Prompt)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

**For Ninja (cross-platform):**
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

**Notes:**
- For a Debug build use `-DCMAKE_BUILD_TYPE=Debug` (or `--config Debug` for multi-config builds).
- If you change critical CMake options, remove the build directory (or delete `CMakeCache.txt`) and reconfigure.
- On Windows, if you use MSYS2/MinGW, run the commands from the appropriate MinGW shell so the correct toolchain is picked up.

## Running the executable

After a successful build the engine binary will be located in the chosen build directory. On Unix-like systems the binary is typically named `QuakeCore`, on Windows `QuakeCore.exe`.

**Examples:**

```
# Copy your id1/ directory next to the built binary and run from the build dir (Unix)
cp -r /path/to/id1 build/
cd build
./QuakeCore
```

**On Windows (PowerShell / CMD):**
```
# From project root (Visual Studio generator example)

# Copy your id1/ directory next to the built binary and run from the build dir
xcopy /E /I path\to\id1 build\Release\id1\
build\Release\QuakeCore.exe
```
