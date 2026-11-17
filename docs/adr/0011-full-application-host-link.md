# ADR 0011: Link the complete shared application in the host wrapper

Status: accepted

The host executable links the upstream Application, Services, System, Foundation, and UIFramework sources with the bundled Braids, libsamplerate, TinyXML2 adapter, and yxml sources. Hardware-only Advance adapter symbols are supplied by wrapper-owned host files. The entry point installs the filesystem, event, timer, audio, MIDI, and sample services before `Application::Init()`.

Some upstream source currently requires host compiler definitions for firmware section attributes and `strlen`, plus a `HOST_TEST` definition for a host-aligned sample load buffer. These are build-time compatibility shims, not edits to the pinned submodule. The portability job will isolate or propose upstream fixes for them.

The headless smoke run checks a rendered firmware frame and dispatches a Down key through the firmware window; a changed frame hash is its navigation assertion. `--screenshot` captures the first rendered view for visual review.
