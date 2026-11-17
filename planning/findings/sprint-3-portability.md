# Sprint 3 portability attempt

## macOS

The complete desktop target builds with Apple Clang and the application smoke and compatibility checks pass. `Application::Init()` renders the Song view and Down changes the frame hash.

## Linux

An Ubuntu 24.04 arm64 container was provisioned with GCC 13.3, CMake and `libsdl2-dev`. CMake configuration succeeded. The first source compile failed because the temporary global `strlen=__builtin_strlen` definition prevented the standard `<cstring>` header from exporting `strlen`; the error appeared in `AppWindow.cpp` and `PathUtils.h`. The wrapper was changed to use `-include cstring` on non-Apple builds and to restrict the macro to Apple. The macOS build passed after that change. A second Linux build could not be run because the local Docker action was rejected by automatic approval review after its usage limit was reached. The Linux fix is unverified.

## Windows

A Windows GNU target CMake attempt compiled its basic C and C++ probes with local Clang. The generated project could not link: the installed `SDL2::SDL2` package is a macOS package and has no Windows `IMPORTED_IMPLIB`. The machine also lacks a Windows SDK / MinGW runtime. Windows application sources were therefore not compiled. The shared upstream sources reference POSIX `strcasecmp`, `strncasecmp`, and `<strings.h>`, which will need a narrow compatibility layer or upstream source fix once a Windows toolchain is available.

## Patch candidates and next check

`patches/0001-constexpr-literal-lengths.patch` replaces template-argument `strlen` calls with `sizeof` expressions in `InstrumentView.cpp`; it is staged outside the submodule for upstream review. `CHAR_WIDTH` in upstream `ScreenConstants.h` also collides with a C23 libc macro on GCC, currently a warning; a namespace or renamed constant is a later upstream change. Keep the submodule pinned while validating any patch in a disposable checkout.

The patch was applied only to a disposable copy under `build/`. CMake's `PICOTRACKER_UPSTREAM_ROOT` option pointed the wrapper at that copy, with `PICOTRACKER_APPLE_STRLEN_WORKAROUND=OFF`. The complete macOS build and project/WAV compatibility check passed. The pinned submodule remains clean. An obsolete model-only CMake target was removed because its source-wide definitions leaked into the full application target.

Next: rerun the Ubuntu build with the changed compile options; provision a Windows cross toolchain plus target SDL2 package and build there. These are required before claiming multiplatform build support.

## Local macOS check

`scripts/check-macos.sh` provides the current repeatable check. It builds the wrapper, runs the host filesystem and input probes, boots the real firmware, checks project/WAV compatibility, and saves a screenshot. Linux and Windows checks are deferred at the user's request. The board job remains open until those builds are revisited.
