# Dedicated Advance source migration

Checked 2026-11-15 on macOS.

## Source and history

- The wrapper now uses `https://github.com/xiphonics/advance-firmware.git` as `upstream/advance-firmware`, pinned to tag `v3.2.1` at `0282da7e50cef0bdc88e4084ae667dfd6ff55f1a`.
- The previous source was `xiphonics/picoTracker` v2.3.1 at `ce77d1f5b31f75a9b772384572ed6705e8c86e3d`. It remains in Git history for comparison.
- The dedicated tag includes the Advance adapter and the FM6, StarLoom and PicoSwarm sources. Its tree has no Git submodules, so recursive checkout has no nested repositories to initialize.
- The application architecture changed substantially between these pins (including the voice and instrument views). The desktop scaffold still demonstrates the drawing seam; it does not prove full application portability.

## License boundary

The dedicated repository's `LICENSING.md` says inherited baseline code retains BSD-3-Clause terms, new original contributions default to GPL-3.0-only, and mixed files carry both. It also states that its third-party inventory is not a complete compatibility audit. The wrapper currently produces a local prototype only. Before distributing a combined desktop binary, audit every included component and asset and provide the required license notices and corresponding source. The Advance bitmap font has separate terms noted in the earlier license review.

## Host changes

- The model and UI compile probes use the new source path. `Groove.cpp` needs the same temporary host `section(x)` workaround already used for `Project.cpp`.
- `AppWindow` now includes the Advance display API and bitmap error screens. A wrapper-owned display declaration shim lets the compile probe avoid the STM32 display header. It is not a runtime display implementation.
- The recording shim now declares `CancelRecording()` and uses the new 32-bit gain signatures.
- `System` adds `GetDeviceIdentitySeed()`. The desktop scaffold returns a fixed placeholder seed; persistence and user-visible device identity behavior need review before running the full application.

## Verification

From the wrapper root:

```sh
git submodule update --init --recursive
/opt/homebrew/bin/cmake -S . -B build/advance-v3-clean -DCMAKE_BUILD_TYPE=Debug
/opt/homebrew/bin/cmake --build build/advance-v3-clean -j4
SDL_VIDEODRIVER=dummy build/advance-v3-clean/picotracker_desktop --smoke
git -C upstream/advance-firmware status --short
```

The clean configure, build (including `AppWindow` and `RecordView` object probes), and smoke check passed on macOS. The submodule status was clean. Actual firmware UI boot, project/sample compatibility, Linux and Windows builds remain Sprint 3 work.
