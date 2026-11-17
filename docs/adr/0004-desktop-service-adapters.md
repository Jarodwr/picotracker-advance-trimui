# ADR 0004: Implement host services against upstream interfaces

Status: Accepted (2026-11-15)

## Context

The Advance boots by installing implementations of `System`, `I_GUIWindowFactory`, `TimerService`, `FileSystem`, `MidiService`, `Audio`, and `SamplePool`. Its display implementation uses an upstream `I_GUIWindowImp` interface and an Ubuntu Mono-derived bitmap font. The host prototype needs to prove those interfaces can be implemented without editing the submodule.

## Decision

Provide a `HostSystem` implementation of the upstream `System` interface using a monotonic host clock, host randomness and explicit unsupported battery/bootloader behavior. Provide an `SDLWindow` implementation of `I_GUIWindowImp` using SDL2 and the pinned Advance bitmap font. Keep SDL types out of the shared application source. The prototype installs `HostSystem` and exercises drawing through `SDLWindow`.

## Consequences

The desktop executable now uses actual upstream service and drawing interfaces, and links the upstream scale calculation. It still renders a temporary host view; the upstream `Application` and `AppWindow` are not initialized. The font license must accompany any distributed build. `HostSystem::Micros()` wraps after roughly 72 minutes because the interface returns `uint32_t`; this matches the interface width and will need rollover handling where consumers compare timestamps.
