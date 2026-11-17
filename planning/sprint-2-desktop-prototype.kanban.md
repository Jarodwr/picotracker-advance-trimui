# Sprint 2: Desktop feasibility prototype

## To Do

## In Progress

## Blocked

## Review

## Done

#### Plan the next implementation milestone
<!-- id: s2-next -->
Turn prototype findings into prioritized work for audio, UI, storage, compatibility and additional platforms. Identify upstream contributions or patches and their maintenance cost.
<!-- priority: medium -->
<!-- workload: normal -->

Completed: Sprint 3 board prioritizes host services, real application boot, compatibility and portability with explicit job dependencies. ADR 0005 records the runtime seam and upstream patch candidates.



#### Verify and demonstrate the prototype
<!-- id: s2-verify -->
Run the host build and a repeatable smoke check, capture the output or reproduction steps, and document what is functional, stubbed or blocked. Check that the submodule remains clean.
<!-- priority: high -->
<!-- workload: normal -->

Completed: CMake build, dummy SDL smoke, orchestrator validation and whitespace check passed; the upstream submodule is clean at the pinned commit. Reproduction steps and limitations are in the Sprint 2 status.



#### Check project and sample handling
<!-- id: s2-compatibility -->
If the UI/input seam works, use a version-matched fixture to exercise project or sample loading without modifying original data. Record compatibility and memory-limit observations; do not claim current-device parity without evidence.
<!-- priority: high -->
<!-- workload: hard -->

Completed as a source-level check because the firmware UI seam is not yet running: the pinned tree has no version-matched project fixture, so no load or round trip was attempted. The Advance pool has fixed 32 MiB + 16 MiB stores and separate count/offset limits. Executable compatibility checks are tracked by `s3-compatibility`. See ADR 0006.



#### Run an upstream Advance code path
<!-- id: s2-execution -->
Build and execute the pinned public Advance application's UI/input path using the host adapters. Record the first visible view and one working key, or exact compile/runtime blockers. This does not establish current firmware feature parity.
<!-- priority: critical -->
<!-- workload: extreme -->

Completed as a blocker investigation: `AppWindow.cpp` and `RecordView.cpp` compile in the host probe, but the firmware UI cannot execute until filesystem, audio, MIDI, timer and sample-pool services and SDL event input are installed. No firmware view or key event is claimed. See the Sprint 2 status and Sprint 3 boot job.



#### Prove a hardware boundary on the host
<!-- id: s2-adapter -->
Implement a minimal desktop window and input adapter at the Advance layout size. Draw an initial application view and process one navigation key; document stubs and unsupported device features.
<!-- priority: high -->
<!-- workload: hard -->

Completed: the 720×720 SDL scaffold renders through the upstream drawing interface and Up/Down moves the selected row. It is a host view, not a firmware view. Recording remains unsupported. See ADR 0004 and ADR 0005.



#### Establish the upstream submodule
<!-- id: s2-submodule -->
Use the initialized project and official upstream submodule pinned to `v2.3.1` (`ce77d1f5…`). Document recursive checkout and update steps; keep upstream changes out of the submodule working tree.
<!-- priority: critical -->
<!-- workload: normal -->

#### Create a reproducible host build
<!-- id: s2-build -->
Create a wrapper-owned host build around selected shared upstream sources and required dependencies. First prove one shared library and host service compile on macOS; document prerequisites and Windows/Linux portability gaps.
<!-- priority: critical -->
<!-- workload: hard -->
