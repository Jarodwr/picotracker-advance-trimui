# ADR 0006: Keep desktop sample capacity separate from hardware storage

Status: Accepted (2026-11-15)

## Context

The pinned Advance adapter reserves two fixed sample stores of 32 MiB and 16 MiB in SDRAM sections (`advSamplePool.h` and `.cpp`). Its loader tracks 32-bit offsets, checks whether each converted sample fits a single store, and `SamplePool` caps the sample count through `MAX_SAMPLES`. A desktop computer can generally provide more memory, but the existing data structures and loader do not support arbitrary capacity.

## Decision

Implement a desktop sample pool with configurable, bounded host memory in a later milestone. Keep file and project formats versioned against the pinned upstream source. Do not change the Advance adapter's fixed arrays or claim unlimited sample memory. Raise defaults only after testing loading, playback, allocation failure, and project round trips.

## Consequences

Desktop capacity can exceed the device's 48 MiB only after the host pool exists. Per-sample and sample-count limits, integer widths, and format compatibility still need explicit checks. This policy preserves the hardware implementation inside the submodule.
