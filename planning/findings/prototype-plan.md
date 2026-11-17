# Desktop feasibility prototype plan

## Source and scope

Use the official submodule pinned to `v2.3.1` (`ce77d1f5…`). This proves whether the public Advance application can run on a host. The source is missing synths described by the current Advance manual, so the prototype cannot demonstrate current firmware parity. Do not claim to load current projects faithfully.

## Build strategy

Add a wrapper-owned CMake target under `platform/desktop`; keep upstream files untouched. Select shared application, UI, service, system and foundation libraries from the submodule. Provide host implementations for the interfaces installed by `advSystem::Boot()` instead of linking STM32 HAL, FreeRTOS or Advance startup code. Record any unavoidable source change as a narrow patch or proposed upstream interface change.

Start on macOS with SDL for window, input and audio. Choose SDL2 or SDL3 during dependency setup based on installed packages and API fit; keep SDL calls inside the host adapter so Windows and Linux can use the same source. Use a dedicated test data directory for host filesystem access. Preserve required notices for upstream code and font assets.

## Milestones and acceptance

1. **Compile seam:** Configure the wrapper CMake target and compile one shared library plus a host service. Capture MCU assumptions exposed by the compiler.
2. **UI/input:** Create an Advance-sized window, draw an initial view and process one mapped navigation key. This is the first runnable milestone.
3. **Audio:** Add host output at the source engine's rate and check continuous playback. Keep MIDI stubbed initially.
4. **Project fixture:** Load a copy of a version-matched fixture from a test directory, observe sample usage and save only to a copy. Check that the submodule is clean.

A successful UI/input slice is enough to validate the central architecture. A later increment can add audio and compatibility.

## Known risks

- `ADV` branches in shared code may still depend on STM32 or FreeRTOS.
- The 48 MiB two-pool model and signed WAV sizes make expanded sample memory a separate task.
- Current Advance project interchange cannot be presumed from this older public source.
- Fonts and dependencies need notices in any distributed build.

Recommendation: proceed with the public-source UI/input feasibility prototype while treating current Advance parity as a separate source-access decision.
