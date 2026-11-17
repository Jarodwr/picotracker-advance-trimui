# Sprint 4 runtime contract findings

These findings refer to the pinned Advance v3.2.1 source under upstream/advance-firmware/sources.

- Services/Audio/AudioConstants.h fixes the firmware rate at 44,100 Hz. Services/Audio/AudioOutDriver.cpp converts float stereo rendering to signed 16-bit PCM and uses AudioDriver::AddBuffer. AudioDriver.h limits one generated chunk to MAX_SAMPLE_COUNT 1875 frames and has a two-entry buffer pool.
- Application/Mixer/MixerService.cpp registers as an AudioOut observer. AudioDriver::OnNewBufferNeeded synchronously reaches MixerService::Update, which renders the next chunk under its lock. The Advance hardware adapter in Adapters/adv/audio/advAudioDriver.cpp performs this work on a dedicated firmware audio task, then flushes MIDI.
- Services/Midi/MidiService.cpp flushes queued output on ADET_DRIVERTICK. Its device settings select outList_[0], outList_[1], or both without a size check. The current silent service registers no output device, so a persisted nonzero MIDI output setting is a boot risk to address during integration.
- Services/Midi/MidiInDevice.cpp calls Player from note and transport handlers. A native CoreMIDI callback therefore needs a queue before invoking this parser. MidiInDevice.h exposes processMidiData for a derived adapter, and Services/Midi/MidiInputSource.h has only Trs and Usb input source categories.
- MidiService.h has fixed vectors of two input and two output devices. This first macOS backend should expose one chosen endpoint per direction plus safe unavailable output slots, with hotplug handled by rescanning rather than changing vector size from a native callback.

ADR 0013 records the compatibility layer, macOS composition, future Linux/Windows stubs, and the thread ownership decision. The later implementation and validation jobs must confirm the proposed queued SDL audio timing and CoreMIDI timestamp conversion on actual macOS devices.
