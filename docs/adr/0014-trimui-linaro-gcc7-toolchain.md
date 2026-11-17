# ADR 0014: The Trimui Linaro GCC 7.5 toolchain cannot build the firmware; use a newer cross compiler

Status: accepted (C-16). Its recommendation was carried out in ADR 0015-trimui-cross-toolchain; the "Not verified" items below describe C-16 as written.

## Question

The vendor/union TG5040 toolchain is Linaro aarch64 GCC 7.5.0 with an SDL2 2.26.1 sysroot. Can it build the pinned Advance source and the desktop wrapper?

## Method

The vendor toolchain was not available. The check used `ubuntu:18.04` for arm64 in Docker, which ships `g++ (Ubuntu/Linaro 7.5.0-3ubuntu1~18.04) 7.5.0` (aarch64, glibc 2.27). That is the same compiler release, so language and library support match, but the sysroot differs (SDL2 2.0.8 headers, Ubuntu libstdc++).

Because GCC 7 has no `-std=c++23`, the wrapper's `CMakeLists.txt` was edited in a scratch copy to C++17 (the newest standard GCC 7 supports). CMake configuration was run with `PICOTRACKER_UPSTREAM_ROOT` pointing at the pinned submodule, and the build ran with `make -k`. `picotracker_full_compile_probe` and `host_filesystem_check` were the failing targets.

## Result: it does not build

The upstream firmware declares `CMAKE_CXX_STANDARD 23` (`sources/CMakeLists.txt`). GCC 7.5 fails even with the standard lowered to C++17:

| Failure | Files | Cause |
| --- | --- | --- |
| `sorry, unimplemented: non-trivial designated initializers not supported` | `Application/Mixer/MixerService.cpp`, `Application/Player/AudioVoice.cpp`, `Application/Player/Player.cpp`, `Application/Views/BaseClasses/UIFM6ParamField.cpp`, `Application/Views/ViewData.cpp`, `Services/Audio/AudioMixer.cpp`, `Services/Audio/AudioOutDriver.cpp` | The firmware uses C++20 designated initializers. GCC 7 supports them only as a C99 extension for trivial in-order cases. |
| `fatal error: filesystem: No such file or directory` | wrapper `platform/desktop/HostFileSystem.h` (and everything including it) | libstdc++ in GCC 7 lacks `<filesystem>` (only `<experimental/filesystem>` with `-lstdc++fs`). |

Every other file in the probe compiled. Pinning the firmware to C++17 is therefore not a route, and patching seven firmware files is out of bounds under ADR 0001.

## Decision

Do not build the TG5040 target with the vendor Linaro GCC 7.5. Use a newer aarch64 cross compiler (GCC 13 or later, which also covers C++23) and keep compatibility with the device by:

- targeting the device's glibc with an old-glibc sysroot (the vendor sysroot's glibc, around 2.27 or older; confirm on the vendor image), for example a crosstool-ng or Bootlin build, or Zig's `aarch64-linux-gnu.2.27` target;
- linking `libstdc++` and `libgcc` statically (`-static-libstdc++ -static-libgcc`), so the newer C++ runtime does not need to exist on the device;
- taking SDL2 headers and the shared library from the vendor 2.26.1 sysroot, since the device provides SDL2 itself.

## Not verified

Status after ADR 0015 (trimui-cross-toolchain): the GCC 13 plus old-glibc approach was built and smoke-tested in a glibc 2.23 container, and SDL2 is linked statically instead of taken from the vendor sysroot. The device's glibc is reported as 2.33. Nothing has been run on the device. The original list follows.

- A GCC 13+ cross compiler against an old-glibc sysroot was not built or run here. The recommendation is untested.
- The vendor sysroot's exact glibc version was not inspected.
- SDL2 2.26.1 headers were not used; only the wrapper's non-SDL sources were exercised, with SDL2 2.0.8 headers.
- Nothing was run on a device.
