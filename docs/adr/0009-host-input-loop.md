# ADR 0009: Dispatch desktop keys as firmware pad events

Status: Accepted (2026-11-15)

## Context

The Advance `AppWindow` consumes `ET_PADBUTTONDOWN` and `ET_PADBUTTONUP`, while the hardware event loop maps physical switches to `GUIEventPadButtonType`. Its timers use the upstream `TimerService` interface. Desktop keyboard events must enter that same view path.

## Decision

Provide `HostEventManager` and `HostWindowFactory` against the upstream interfaces. Translate SDL arrows and action keys into pad events, dispatch through `GUIWindow`, and drive clock ticks and timer observers from a monotonic host loop. Keep the SDL scaffold for smoke checks while the real application boot is built.

## Consequences

A focused check demonstrates Down key press/release reaching an upstream `GUIWindow` probe and a timer notifying its observer. The scaffold installs the services and pumps them, but the firmware `AppWindow` is not yet linked; its cursor movement is an acceptance criterion of the boot job. The current key mapping is provisional and needs usability review once the real view runs.
