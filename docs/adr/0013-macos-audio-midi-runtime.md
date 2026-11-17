# ADR 0013: Compose host backends behind the firmware service contracts

Status: accepted

## Context

The desktop wrapper already installs the firmware's System, FileSystem, TimerService, GUI window factory, Audio, MidiService, and SamplePool interfaces. Its platform headers also replace hardware-only display, USB, and recording symbols. The pinned Advance source remains a clean submodule.

The next milestone needs real macOS audio and MIDI while keeping Linux and Windows possible later. A new parallel copy of the firmware service API would add translation and lifetime rules without solving a current requirement.

## Decision

Treat the wrapper-owned adapters and hardware shims as the compatibility layer. Keep reusable filesystem, timer, SDL window/input, silent fallback, and audio driver code in a common host area. A macOS backend composes those services with CoreMIDI devices and selects them at startup. Select exactly one platform backend at build time; the firmware application sees its existing interfaces only.

Prepare separate Linux and Windows backend entries when those platforms are resumed. Until implemented, each must return explicit unsupported capability results, register no pretend hardware, and fail a platform acceptance check. The silent audio fallback remains available for boot and tests but does not count as working playback. A platform status record should expose audio, MIDI input/output, and recording availability to diagnostics and UI settings.

For audio, reuse upstream AudioOutDriver and implement an SDL AudioDriver rather than duplicating rendering and PCM clipping. Request 44,100 Hz, signed 16-bit, interleaved stereo output with allowed_changes set to zero, so SDL presents that format and converts to a different hardware format when needed. Fail cleanly if the device cannot open. SDL_QueueAudio copies buffers. A host event-loop pump asks the driver for a buffer on the firmware thread, queues it, then notifies the driver tick so MIDI output flushes after the corresponding audio chunk. Keep a bounded queue ahead of playback and pump more often than the UI clock while playing. The SDL audio callback never calls firmware objects. Stream time is estimated from submitted frames minus queued bytes; SDL's queued-byte count does not include bytes already handed to hardware, so validate drift against a monotonic clock.

For MIDI, CoreMIDI callbacks enqueue bounded timestamped bytes and device-change notices. The host event loop drains them before calling upstream MidiInDevice parsing or Player paths; outgoing messages are sent from that same firmware thread. Teardown stops callbacks before destroying the upstream device. Virtual CoreMIDI ports provide test endpoints without physical hardware.

Register safe output slots before MidiService::Init. Its saved output setting selects slot 0, slot 1, or both without checking list length. A no-device slot must drop messages and identify itself as unavailable; an empty list is unsafe when an old configuration selects MIDI output. Upstream currently allows two input and two output device objects, and MidiInputSource only distinguishes Trs and Usb. The macOS backend starts with one selectable CoreMIDI endpoint per direction, maps it to the Usb source category, and documents this limit instead of claiming arbitrary endpoint support.

## Consequences

The common layer owns the firmware-facing API and thread boundary; each OS backend owns only its native device lifecycle. Audio and MIDI implementation jobs can edit separate files, followed by one integration job for startup and CMake. The pinned submodule stays unmodified. Linux and Windows stubs are scheduled with the later platform sprint, after macOS feature completeness and release readiness.

SDL's [queued audio](https://wiki.libsdl.org/SDL2/SDL_QueueAudio), [queue size](https://wiki.libsdl.org/SDL2/SDL_GetQueuedAudioSize), and [device format](https://wiki.libsdl.org/SDL2/SDL_OpenAudioDevice) contracts support this design. Apple's [CoreMIDI guidance](https://developer.apple.com/documentation/coremidi/incorporating-midi-2-into-your-apps) confirms that receive callbacks can run on a high-priority thread and should hand work to a queue.

## Implementation notes (startup integration)

`MacBackend` (platform/macos) composes `HostAudio` (SDL, falling back to the silent output if no device opens) with `CoreMidiIn`/`CoreMidiOut` inside `HostMidiService`, and exposes a `PlatformStatus` record (audio, MIDI in/out, recording). Upstream only starts input devices, so the backend initialises the CoreMIDI input itself. Both output slots are always populated: slot 0 is the hardware destination, slot 1 is a virtual source ("picoTracker Out"); the input is the hardware source plus a virtual destination ("picoTracker In"). This uses the second output slot for the virtual port, which refines the "one endpoint per direction" starting point above. The backend's own virtual endpoints are excluded from hardware selection so output never loops back into input. The host event loop calls the backend pump every 4 ms when a pump is installed.
