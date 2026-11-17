# Trimui Smart Pro: recording, end to end (C-32)

Date: 2026-11-17. Device: Trimui Smart Pro, NextUI 20261013-0, TinaLinux 4.9.191 aarch64, ALSA card 0 `audiocodec`. The binary was cross-built from this branch (`scripts/build-trimui.sh`) and installed with `scripts/deploy-trimui.sh`.

**Owner decision applied:** the Smart Pro records **mono** from its one built-in mic (MIC1, left channel, C-27). Where a stereo file comes out, the mono signal must be duplicated L to R. `tests/trimui/wav_check.py` enforces this: a mono file, or a stereo file whose right channel equals the left.

## How the checks work

- `tests/trimui/record_check.sh --ssh root@HOST [--source mic|line] [--seconds N] [--label NAME]` deploys the pak, copies a cross-built gamepad injector (`tests/trimui/uinput_inject.c`, a C port of `uinput_inject.py` because the device has no Python) and `record_device.sh` to the device's `/tmp`, and runs the pak's own `launch.sh`. The injector makes a virtual pad through `/dev/uinput`. Presses: Edit+Play (B+Start) opens the Record screen, Play starts, Play stops, then the Select+Start+L1+R1 chord exits. The WAV, both app logs and ALSA mixer snapshots (before, during, after) are copied back and checked on the Mac. Afterwards the helpers in `/tmp` and the `REC01.wav` it created are removed from the card.
- `tests/trimui/emulated_smoke.sh` has a new step 7 (arm64 Ubuntu 16.04 container, glibc 2.23): the packaged pak records and the WAV must appear under the data root, load, and carry signal. The SDL dummy capture driver delivers silence and the firmware's record threshold (10 of 255) never fires on silence, so no file is written with it. The step therefore uses SDL's ALSA backend with an `~/.asoundrc` whose default capture is the ALSA `file` plugin reading a paced 440 Hz tone from a FIFO. This replaces the mic; it does not test the codec.

## Results

| Check | Result |
|---|---|
| Emulated, glibc 2.23, whole `emulated_smoke.sh` | pass. `REC01.wav` under `PicoTracker/recordings/`, 44100 Hz, 16-bit, 3.99 s, RMS 5656 (tone), sample editor loaded it, app exited 0 on the chord |
| Device, source Mic, 5 s, no headset (two runs) | pass. mono, 44100 Hz, 16-bit, 5.97 to 6.04 s (5 s record plus about 1 s of press latency on each side), RMS 1977 and 3493 (room sound), peak 18462 and 32767. App log: sample editor loaded the file |
| Device, ALSA mixer before vs after | identical in the last two runs. A run that started right after a `killall` of the app showed `DAC volume` 160,160 before and 0,0 after: that is the playback codec left up by the killed process, not a capture control. The launcher's save/restore of `ADC volume`, `MIC1/MIC2 gain volume`, `ADC Swap` was in use and nothing changed. No capture control was changed by the app or the check |
| Device, playback and capture together | the app's SDL ALSA playback is open for the whole run and the capture opens when the Record screen opens. **One** `ALSA lib pcm.c: snd_pcm_recover underrun occurred` appears per run, at boot or on entering the Record screen (log offset below the one taken just before Play); **none between pressing Play to start and Play to stop** (0 in the window, 3 of 3 runs). Overrun counts of the capture ring are not logged by the app, so a dropped capture block would not show; the 5.97 to 6.04 s lengths for a 5 s record say no large gap occurred |
| Device, source Line In (the firmware default, `RECORDSOURCE=1`) | **fail on the mono policy**: a 2-channel file with the signal on the left (RMS 1979) and low noise on the right (RMS 6.9, peak 43); 259,900 of 266,175 frames differ. Backlog card added (see below) |

## Not run

- **With a headset.** Nobody was at the device to plug one in, and the script cannot tell. `record_check.sh --label headset --source mic` is the command to run with one plugged in; the jack is the input device `audiocodec sunxi Audio Jack`. Which pin feeds MIC2 and whether it lands in the left channel is therefore not characterised.
- **The real buttons.** The device's own pad (`js0`, shown to SDL as 'Xbox 360 Controller', 11 buttons) did not take events written to its evdev node, so the virtual pad (13 buttons, SDL numbering of ADR 0016) drives the app. This proves the recording flow, not the real button mapping.
- **Changing the source in the UI.** Edit+Right on the Record screen did not change the source in the one try, so the check writes `RECORDSOURCE` into `.config.xml` before launch and puts the saved copy back afterwards (the file is in the data root).

## Things found on the way

- The pak's `launch.sh` does not export `SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1`. A virtual pad's events were dropped until it was set, on the device as in the container. The real pad was not tested (see above). The check exports it itself. If real pad presses are ever dropped for the same reason, that line belongs in `launch.sh`.
- `RecordView` logs `source: Mic` for every non-zero value of `RECORDSOURCE`, including Line In (the log line in the firmware tests `== 0`). Do not read that line as the source.
- `scripts/deploy-trimui.sh --ssh` failed twice before this card: modern `scp` refuses to copy a directory onto a path that does not exist (it had already deleted the old pak), and the script could not use a firmware checkout other than `upstream/advance-firmware` (a worktree has an empty one). It now creates the directories first and copies their contents, and honours `PICOTRACKER_UPSTREAM_ROOT`.

## Card on the board

- Main gained `PICOTRACKER_LINEIN_MONO=1` (set by the pak launcher, commit 1df1325) after these device runs, which duplicates L to R for Line In. The Line In result above predates it, was measured without it, and has not been re-run on the device. The card below may already be covered; re-run `record_check.sh --source line` to find out.
- Line In writes a stereo file whose right channel is not a copy of the left; consider defaulting the source to Mic on this board. Fix is in `platform/desktop/HostRecord.cpp`, which this card was not allowed to touch.
