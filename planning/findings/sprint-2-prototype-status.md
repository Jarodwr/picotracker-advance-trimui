# Sprint 2 prototype status

## Demonstrated

- The official picoTracker submodule remains pinned to `v2.3.1` and clean.
- CMake builds the full shared `Application/Model` static library on Apple Clang with two temporary, file-scoped compatibility definitions. The complete model target includes Chain, Config, Groove, Mixer, Phrase, Project, Scale, Song and Table.
- The SDL2 executable opens a 720×720 window (or uses SDL's dummy video driver for a headless smoke run), implements the upstream `I_GUIWindowImp` drawing contract, uses the pinned Advance bitmap font, installs a host `System` service, and calls the upstream scale calculation while Up/Down changes the selected row. Escape exits.
- A separate `picotracker_application_probe` object target compiles the pinned upstream `AppWindow.cpp` and `RecordView.cpp` on the host. Wrapper-owned `platform.h` and recording headers isolate hardware dependencies; recording functions currently return unsupported/inactive results. This target does not execute the firmware UI.
- `cmake -S . -B build/host -DCMAKE_BUILD_TYPE=Debug`, `cmake --build build/host -j4`, and `SDL_VIDEODRIVER=dummy build/host/picotracker_desktop --smoke` completed successfully on macOS.
- Final verification on 2026-11-15: the CMake build (including the application probe), dummy-video smoke run, `python3 orchestrator.py validate` (`Valid: 6 jobs, no dependency cycles`), `git diff --check`, and a clean submodule status all passed. The submodule commit was `ce77d1f5b31f75a9b772384572ed6705e8c86e3d`.

## Still required for the board's original acceptance criteria

- The SDL view is a host scaffold. It does not construct `Application`/`AppWindow`, so it does not yet render the firmware's actual UI or exercise its event dispatch.
- Host timer, filesystem, MIDI, audio and sample-pool services are not installed. Project/sample loading and audio playback are untested.
- A syntax audit of 127 shared translation units compiled 116 with host Clang using temporary compatibility macros. Wrapper shims now cover the hardware `platform.h` and FreeRTOS recording header for the view probe. Other barriers included bundled sample-rate headers, Pico-specific Braids DSP code, and two template implementation `.cpp` files that are not standalone translation units. This audit is diagnostic, not a full host build.
- `Application::Init()` calls `PersistencyService`, `FileSystem`, `AppWindow::Create`, and `Audio::Init`. `AppWindow` also initializes MIDI and its views use timers and the sample pool. The host has only `System` and the drawing interface installed. These missing factories and their transitive implementations are the current execution blockers; no actual firmware screen has been run.
- The pinned repository contains no version-matched `.lgptsav` project fixture. Files under nested third-party SDK tests are not picoTracker project fixtures. Project and sample loading remain untested.
- The Advance sample pool reserves 32 MiB and 16 MiB in fixed hardware memory sections. Host memory can be larger only after a dedicated pool is implemented; sample count, per-sample fit, and 32-bit offsets are separate limits. See ADR 0006.
- Windows and Linux builds have not been run.

## Next implementation slice

Implement the host filesystem, audio, MIDI, timer and sample-pool services and an SDL event manager so `Application::Init()` can run. Add a version-matched project and sample fixture to verify read and round-trip behavior. Replace the temporary compiler macros with narrow upstreamable fixes before distribution. Keep the current SDL scaffold as a smoke target until the real view can replace it.

The prioritized jobs and explicit dependencies are on the Sprint 3 board. Its critical path is filesystem/input/audio-MIDI/sample-pool services, then application boot, then compatibility and portability checks. The narrow compiler workarounds and wrapper header shims are candidates for upstream contributions; the host service implementations remain desktop-specific.
