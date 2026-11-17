# Linux build (card C-8)

Result: on Ubuntu 24.04 arm64 (GCC 13.3, SDL 2.30, run in Docker on Apple silicon) the wrapper builds, the firmware boots with `SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy` (audio reports `dummy`), and the host filesystem, host event and project/WAV compatibility checks pass. The macOS build, `host_*` checks, compatibility check and smoke run still pass. Run `scripts/check-linux.sh` (on macOS it launches the container; on Linux it builds in `build/linux-check`).

## Changes
- `cmake/linux.cmake`, included once at the end of `CMakeLists.txt`: adds `platform/linux/LinuxBackend.cpp` and defines `PICOTRACKER_LINUX_BACKEND` for `picotracker_desktop`.
- `platform/linux/LinuxBackend.{h,cpp}`: same interface as `MacBackend` (real SDL audio, silent fallback) with explicitly unsupported MIDI: every slot is a null device and `Status()` reports MIDI in/out unavailable.
- `scripts/check-linux.sh`.
- **`platform/desktop/main.cpp` (outside this card's owned files, C-3's):** the include and the three `__APPLE__` backend blocks also accept `PICOTRACKER_LINUX_BACKEND`. This is unavoidable; without it Linux takes the generic `#else` path, which aborts at exit (below). C-3 should review it.

## Findings
- The earlier `strlen` failure is gone: non-Apple builds already use `-include cstring`. Patch 0001 applied to a disposable copy (`PICOTRACKER_UPSTREAM_ROOT`) also passes the full Linux check. It is not needed for Linux; the submodule stayed clean.
- **Bug in `platform/desktop/HostMidi.h` (C-3, not fixed here):** `HostMidiService` puts its internal null `HostNullMidiOut`/`HostNullMidiIn` devices in `outList_`, but upstream `~MidiService()` calls `Close()` -> virtual `stopDevice()` after those members are destroyed, so the process aborts with "pure virtual method called" at exit whenever a null output slot is used (the old non-Apple `#else` path in `main.cpp`; any macOS run where a CoreMIDI slot is null). `LinuxBackend` works around it by owning the null devices itself, declared before `midi_`. Suggested fix: have `HostMidiService` clear `outList_`/`inList_` in its own destructor after `Close()`, or make it own devices that outlive the base destructor.
- `CHAR_WIDTH` from upstream `ScreenConstants.h` redefines the glibc `limits.h` macro (GCC 13 warning only, as predicted in ADR 0012).

## Remaining gaps
- No Linux MIDI (ALSA/JACK) or audio capture/recording; `PlatformStatus` is duplicated between `MacBackend.h` and `LinuxBackend.h` and should move to `platform/desktop` when a third backend appears.
- Validated only on arm64 with dummy drivers; no real audio device, X11/Wayland window, or x86_64 run. No cross-compile for the Trimui device yet.
- Windows is untouched.
