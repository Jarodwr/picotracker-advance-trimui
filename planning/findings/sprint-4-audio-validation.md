# Sprint 4 audio validation (macOS)

Card C-4. Check: `tests/macos_audio_check.cpp` (build line in its header).
It drives the production `SDLAudioDriver` the way the firmware does
(`Init`/`Start`, `Pump` on one thread, buffers delivered through the upstream
`AudioDriver` pool, upstream `ADET_BUFFERNEEDED`/`ADET_DRIVERTICK` events) with
a repeatable 440 Hz sine (amplitude 8000/32767, 735-frame chunks).

## Environment

- macOS (Darwin 25.5.0, arm64), SDL 2.32.74 via Homebrew, CoreAudio backend.
- Output devices SDL enumerated: `S34J55x` (HDMI), `USB Audio CODEC`, `CM01`,
  `MacBook Pro Speakers`. The check opened the system default (`"default"`).
  System devices run at 48 kHz; the driver requests 44.1 kHz and SDL converts.
- Driver reported: rate 44100, 2 channels, 1024-frame device buffer (about
  23 ms).

## Results (full 60 s run, 0 failures)

| Check | Result |
| --- | --- |
| Open default device | real CoreAudio device, no silent fallback |
| Non-silent signal | peak 8000 handed to open device; 134 505 frames over 3 s; stream clock 2.995 s for 3.002 s wall |
| Start/stop x5 | clock resets to 0 on Stop, `Pump` is a no-op while stopped, restart in step (0.488-0.511 s per 0.5 s) |
| 60 s timing | wall 60.001 s, stream 60.000 s, final drift -0.4 ms, max abs drift 26.8 ms (about one device buffer), no starvation in 24 715 polls, max pump gap 23.7 ms |
| Device loss | see below; fallback entered, clock keeps running, Stop/Start/Close clean, `Init` reopens a real device |

The ~25 ms drift wobble is the expected quantisation: `GetStreamTime` is
queued-bytes based, so it moves in device-buffer steps, and the doc comment
already notes it overestimates by up to one buffer. It does not accumulate.

## Limits and what was not verified

1. **Audibility is not machine-verified.** There is no loopback capture. The
   check proves a non-zero signal reached an open CoreAudio device and was
   consumed at real time; a person must still listen (the run announces the
   tone). Which physical output the system default routed to was not recorded.
2. **Device loss is simulated, not a physical unplug.** The check closes the
   SDL device id behind the driver's back, so `SDL_GetAudioDeviceStatus`
   returns `SDL_AUDIO_STOPPED`. Whether SDL's CoreAudio backend reports
   `STOPPED` (rather than staying `PLAYING` while silent) when a USB or HDMI
   device is really removed is unconfirmed; `deviceStillAlive()` depends on it.
   A manual unplug run is still needed. Suggested: run with
   `PICOTRACKER_AUDIO_DEVICE="USB Audio CODEC"`, pull the cable during step 4,
   and look for `deviceLost` in the status.
3. **No version-matched tracker project was played.** The check uses a signal
   through the driver, not an Advance v3.2.1 project through the mixer
   (`AudioOutDriver::Trigger`). Mixer-to-driver behaviour (buffer sizes other
   than 735 frames, clipping, peaks) is untested here.
4. **Loss recovery is not automatic.** After loss the driver stays in silent
   fallback until `Close`/`Init`; a replugged device is only picked up by
   re-initialising audio. Step 6 confirms reinit works.
5. Device enumeration is empty until SDL's audio subsystem is initialised;
   the check lists devices after `Init` for that reason.
6. The check is not in `scripts/check-macos.sh` or CMake (those belong to the
   startup/CMake owner and the acceptance card). It needs a real audio device
   and about 70 s, so it should stay an opt-in check, not a CI gate.

## Defects

None found in production sources. No production files were changed.

## Repeating

Copy the build lines from the header of `tests/macos_audio_check.cpp` (with
`upstream/advance-firmware` initialised), then run `./macos_audio_check 60`.
