# ADR 0012: Keep compiler compatibility in the wrapper until upstream fixes land

Status: accepted

The pinned firmware is immutable in this repository. Host compile options currently replace firmware section placement, use `HOST_TEST` for host-aligned sample-load storage, and compensate for source assumptions about string declarations and constant expressions. Those options belong only to the wrapper targets.

An upstreamable patch is staged in `patches/0001-constexpr-literal-lengths.patch`: two `strlen` calls used as template arguments become `sizeof` expressions. This is a narrow source fix that should remove the macOS `strlen=__builtin_strlen` workaround when it is accepted upstream. The patch is not applied to the submodule.

The patch was applied to a disposable source copy via `PICOTRACKER_UPSTREAM_ROOT`; with `PICOTRACKER_APPLE_STRLEN_WORKAROUND=OFF`, the full macOS application built and passed the firmware smoke and project/WAV compatibility checks. The pinned build also passed after removal of an obsolete model-only CMake probe whose source-wide flags affected other targets.

GCC 13 on Ubuntu first failed because the global `strlen` macro disrupted `<cstring>` declarations. The wrapper now uses a forced `<cstring>` include on non-Apple builds and limits that macro to Apple. A Linux rebuild is required to verify this change. Windows also needs a real cross toolchain and SDL2 import library before application sources can be compiled; POSIX `strcasecmp` and `<strings.h>` use will then need a targeted portability fix.
