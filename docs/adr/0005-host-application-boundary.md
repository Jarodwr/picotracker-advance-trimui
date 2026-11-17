# ADR 0005: Compile the upstream application separately until host services exist

Status: Accepted (2026-11-15)

## Context

The desktop executable currently draws through the upstream `I_GUIWindowImp` interface but uses a small host-owned view. The upstream `AppWindow` constructs every firmware view and `Application::Init()` immediately loads or creates a project and initializes audio. Linking a real view requires more than a window: the boot path installs filesystem, MIDI, audio, timer and sample-pool factories.

## Decision

Keep the functioning SDL scaffold as the executable and add a CMake object target that compiles upstream `AppWindow.cpp` and `RecordView.cpp` with wrapper-owned `platform.h` and recording headers. The host recording implementation reports unsupported recording explicitly. Do not change the submodule or bypass the application boot path to present a partial firmware UI as complete.

## Consequences

The view graph is now checked on every host build. It is not linked or executed. The next milestone must implement host services, then replace the scaffold with `Application::Init()` and verify an actual firmware screen and navigation event. The temporary `section(x)` and `strlen` compiler definitions remain local compatibility debt and should be replaced by narrow upstreamable changes.
