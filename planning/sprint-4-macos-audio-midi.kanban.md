# Sprint 4: macOS audio, MIDI, and app bundle

## To Do

#### Implement SDL audio output
<!-- id: s4-audio-output -->
<!-- depends-on: s4-runtime-contracts -->
Implement an SDL AudioDriver and reuse upstream AudioOutDriver for rendering and PCM clipping. Cover queue timing, start/stop, device loss, and clean silent fallback. Keep startup and CMake edits for the integration job.

#### Implement macOS MIDI devices
<!-- id: s4-midi-devices -->
<!-- depends-on: s4-runtime-contracts -->
Implement macOS MidiInDevice and MidiOutDevice adapters using CoreMIDI. Handle port discovery, hotplug, timestamps, routing, and safe teardown. Keep startup and CMake edits for the integration job.

#### Integrate host audio and MIDI services
<!-- id: s4-service-integration -->
<!-- depends-on: s4-audio-output, s4-midi-devices -->
Install both backends in the real application startup path, wire build dependencies, expose device selection, and retain a working no-device launch.

#### Verify audible playback and timing
<!-- id: s4-audio-validation -->
<!-- depends-on: s4-service-integration -->
Play a version-matched project on macOS through an actual output device. Check non-silent output, stable timing, start/stop, and device loss; record repeatable evidence.

#### Verify MIDI routing and device changes
<!-- id: s4-midi-validation -->
<!-- depends-on: s4-service-integration -->
Use CoreMIDI virtual ports to exercise note/clock input and output through upstream routing. Check reconnect behavior and no-device launch without requiring physical MIDI hardware.

#### Package a local macOS app bundle
<!-- id: s4-macos-bundle -->
<!-- depends-on: s4-audio-validation, s4-midi-validation -->
Build a self-contained local .app with SDL and required notices, launch it outside the build tree, and verify that its data directory survives relaunch.

#### Run macOS release acceptance
<!-- id: s4-macos-acceptance -->
<!-- depends-on: s4-macos-bundle -->
Run the local build and compatibility checks against the bundled app, capture the first view and audio/MIDI results, document installation steps and remaining limitations, and verify the pinned submodule is clean.

## In Progress

## Blocked

## Review

## Done

#### Define host audio and MIDI runtime contracts
<!-- id: s4-runtime-contracts -->
<!-- depends-on: s3-boot, s3-compatibility -->
Trace upstream audio timing, buffers, MIDI routing, and device lifetimes. Record the SDL audio and CoreMIDI adapter contracts, ownership, fallback behavior, and test seams in an ADR before implementation.
