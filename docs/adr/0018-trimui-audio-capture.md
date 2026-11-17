# ADR 0018: Host audio capture adapter

Status: Proposed (2026-11-17); the adapter is wired into recording and ran on the Trimui, mono from the built-in mic (see [planning/findings/trimui-recording.md](../../planning/findings/trimui-recording.md))

## Context

The firmware's recording (RecordView) needs an input signal. The wrapper owns every host service (ADR 0004), so a capture source belongs in `platform/desktop`, next to the SDL audio output. It must work on the Trimui Smart Pro (ADR 0015: statically linked SDL 2.26.1 with the ALSA backend loaded by `dlopen`), on macOS, and under `SDL_AUDIODRIVER=dummy` for the checks.

## Decision

1. **SDL capture device, not ALSA directly.** `SDL_OpenAudioDevice(NULL, iscapture=1)` is used for all platforms, asking for 44.1 kHz, signed 16-bit, 2 channels with no allowed changes (SDL converts, so the adapter always sees that format). No extra library is linked and the same code runs on macOS and under the dummy driver.
2. **Layers** (`HostAudioCapture.h/.cpp`):
   - `HostCaptureSource`: opens a device and pushes blocks to a `HostCaptureSink`. `MakeSDLCaptureSource()` is the real one; the test uses a fake.
   - `HostAudioCapture`: a sink with a preallocated SPSC ring (frame counters, atomics only; default one second of audio). `Read()` drains it from the consumer thread. A block that does not fit is dropped whole and counted (`OverrunBlocks`, `OverrunFrames`), which keeps the consumer the only writer of the read position. `Level()` is the peak absolute sample of the latest block. The callback never allocates or locks.
3. **Unavailable, never a crash.** If the source cannot be opened (no SDL audio, no capture device, wrong format), `Open()` returns false and the state is `Unavailable`; `Read()` returns 0 and `Level()` 0. `Open()` may be retried. `Close()` joins the callback thread (SDL does this in `SDL_CloseAudioDevice`) and is safe in any state.
4. The adapter is compiled into `picotracker_desktop` but not yet called; wiring it into `HostRecord.cpp` is a later card.

## Verified

- Fake-source check (`tests/host_audio_capture_check.cpp`): ring wrap, whole-block overrun counting, level, unavailable (source fails, null source, zero-size ring). With `SDL_AUDIODRIVER=dummy` the adapter opens and delivers silence, closes and reopens. Runs in `scripts/check-macos.sh` and `scripts/check-linux.sh`.
- **On the Trimui (tg5040, NextUI 20261013-0), 2026-11-17**, using the cross-built static-SDL binaries and a throwaway probe (not committed; removed from the card afterwards):
  - `host_audio_capture_check` passes (dummy driver).
  - The probe with SDL's default driver opened a capture device: SDL chose `dsp` (OSS `/dev/dsp`), delivered 124,928 frames in 3 s with 0 overruns and a nonzero input level (peak 11057), then closed cleanly.
  - `arecord -D default -f S16_LE -r 44100 -c 2` on ALSA card 0 `audiocodec` records 44.1 kHz stereo S16 as well. One earlier attempt returned "Device or resource busy" with nothing holding the capture stream, and the next attempts worked; treat a busy device as a possible transient and keep the retryable `Open()`.
  - **`SDL_AUDIODRIVER=alsa` fails on the device** ("Audio target 'alsa' not available") with this static SDL build, although `libasound.so.2` exists on the device. The adapter then reports `Unavailable` without crashing, which also exercised that path live. ADR 0017's suggestion to export `SDL_AUDIODRIVER=alsa` in `launch.sh` must not be followed: leave it unset (SDL picks `dsp`) or set `dsp`.

## Not verified

- Capture while SDL playback is running at the same time (full duplex on `dsp`); the probe captured alone.
- Mixer state: ADC volume and MIC1/MIC2 boost were read-only here and left as found, so level and which microphone or line input is live on the device are not characterised.
- Cause of the missing `alsa` target (C-35): SDL's CMake probes ALSA by linking `-lasound`, the glibc sysroot had no such library, and SDL silently left the backend out (`nm libSDL2.a | grep -ic alsa` gave 0). The Dockerfile now installs a stub `libasound.so.2.0.0` in the sysroot for the probe; the rebuilt `libSDL2.a` has the ALSA backend and dlopens `libasound.so.2`. Not yet run on the device: until a probe confirms `SDL_AUDIODRIVER=alsa` works there, keep `dsp` (the default) as the verified path.
