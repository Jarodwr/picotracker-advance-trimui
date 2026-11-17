# Sprint 3: Run the upstream application

## To Do

## In Progress

## Blocked

#### Validate portability and patch strategy
<!-- id: s3-portability -->
<!-- depends-on: s3-boot -->
Build on Linux and Windows, identify remaining platform code, and prepare narrow upstreamable fixes for temporary host compiler definitions without changing the pinned submodule. Deferred until after macOS feature completeness and release readiness.

## Review

## Done

#### Migrate to the dedicated Advance source
<!-- id: s3-source-rebase -->
Compare `xiphonics/advance-firmware` v3.2.1 with the current pin, review licenses and recursive dependencies, then repin the submodule and restore a buildable host scaffold. Record the result and source-specific risks.

#### Implement a sandboxed host filesystem
<!-- id: s3-filesystem -->
<!-- depends-on: s3-source-rebase, s2-build -->
Map firmware paths such as `/projects` into a dedicated host data directory. Support the upstream `FileSystem` and `I_File` contracts, reject path escape, and preserve project data during read tests.

#### Implement host timer and event input
<!-- id: s3-input -->
<!-- depends-on: s3-source-rebase, s2-adapter -->
Install a host `TimerService`, window factory, and SDL key mapping. Send an Up/Down event through an upstream `GUIWindow` probe and record the resulting cursor movement. The firmware cursor is checked by the application boot job.

#### Implement silent audio and MIDI adapters
<!-- id: s3-audio-midi -->
<!-- depends-on: s3-source-rebase, s2-execution -->
Implement interface-correct silent audio and MIDI services and wire them into the boot path. Add actual SDL audio and MIDI in a subsequent milestone. Report unsupported devices in the UI when the firmware view runs.

#### Implement a bounded host sample pool
<!-- id: s3-samples -->
<!-- depends-on: s3-filesystem -->
Use configurable host memory, explicit allocation failure, and unchanged upstream sample/project formats. Check 32-bit sizes and sample-count limits before raising capacity.

#### Boot Application and verify a real view
<!-- id: s3-boot -->
<!-- depends-on: s3-filesystem, s3-input, s3-audio-midi, s3-samples -->
Replace the scaffold entry point with `Application::Init()`. Capture the first firmware view and one successful navigation event on macOS; keep a headless smoke check.

#### Check project and sample compatibility
<!-- id: s3-compatibility -->
<!-- depends-on: s3-boot -->
Use a project and WAV fixture matching the pinned public source. Load copies from a temporary data directory, verify round trips and capacity errors, and record gaps from current Advance firmware.
