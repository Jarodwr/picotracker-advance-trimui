# ADR 0003: Track upstream host compiler barriers as narrow fixes

Status: Accepted (2026-11-15)

## Context

Apple Clang compiles seven shared Advance model translation units. It rejects `Config.cpp` because `PersistenceConstants.h` uses `strlen()` in a template size. Forcing `strlen` to the compiler builtin allows that file to compile, but that global macro is not a sound product solution. `Project.cpp` then hits `section(".DTCMRAM")` in `SampleInstrument.h`, which is an MCU linker section invalid for macOS Mach-O.

An old `origin/linux-sdl2` branch contains SDL host adapters, but its last commit is from 2024 and predates the pinned Advance source. Treat it as an interface reference, not as a drop-in implementation.

## Decision

Keep the pinned submodule unchanged. Record each blocker with an exact file and compiler result. Prefer upstreamable changes that make constant sizes genuinely constant and gate MCU section attributes by target. For the feasibility build, apply file-scoped compiler definitions (`strlen=__builtin_strlen` for `Config.cpp`, and `section(x)=unused` for `Project.cpp`) so the complete model library compiles. These are temporary build workarounds; do not ship them as the final portability layer. The SDL host scaffold is not the real application UI.

## Consequences

The model library compiles on macOS, but the full application and its service implementations do not yet. The next implementation job is a host service layer and replacement of these workarounds with narrow fixes. The host build remains repeatable without contaminating the submodule.
