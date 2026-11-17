# Trimui Smart Pro: first run on a device

**Status:** the app has been run on the device for recording, sample-memory and audio measurements (see [recording](../planning/findings/trimui-recording.md) and [sample memory](../planning/findings/trimui-sample-memory.md)), but the picture on the panel and the real buttons are **not yet verified**: the device's `/dev/fb0` hung after earlier probes ([display findings](../planning/findings/trimui-display.md), [acceptance](../planning/findings/trimui-acceptance.md)). Device facts (NextUI 20261013-0, tg5040): TinaLinux kernel 4.9.191 aarch64, glibc 2.33, busybox `sh`, about 1 GB RAM, SD card vfat at `/mnt/SDCARD`, no system `libSDL2`, `libdrm` or `libgbm` (only `libasound.so.2`), `/dev/fb0` and `/dev/dri/card0` present, a joystick `js0` and an event device named 'TRIMUI Player1'. Everything else below is **unverified** until a person records it in the results table. The tooling was checked with `shellcheck`, `--dry-run` and `tests/trimui/package_check.sh`; `tests/trimui/emulated_smoke.sh` runs the packaged pak in an Ubuntu 16.04 arm64 container (dummy SDL drivers, virtual uinput gamepad): launcher, seeding, log rotation and the exit chord, but not the real device. The first release supports NextUI only; USB MIDI is out of scope for this board. The layout is ADR 0017, the toolchain is ADR 0015 (`0015-trimui-cross-toolchain.md`), input and display are ADR 0016.

## Install from the CI zip

1. On GitHub open the `linux-arm64` run for the commit, job `trimui-cross`, and download the artifact `PicoTracker-trimui-tg5040-<short commit>`. GitHub wraps the zip in another zip, so unpack that first. For a `v*` tag, take `PicoTracker-trimui-tg5040-<tag>.zip` from the release instead.
2. Extract the PicoTracker zip at the SD card root so that `Tools/tg5040/PicoTracker.pak/` appears. Eject, put the card in the device and start PicoTracker from Tools.
3. **Update:** delete `Tools/tg5040/PicoTracker.pak`, then extract the new zip at the SD root. Projects and samples live in `.userdata/tg5040/PicoTracker/` and are kept.
4. To build the zip yourself: `scripts/build-trimui.sh`, then `scripts/package-trimui.sh`.

## Controls and `input.ini`

Defaults (ADR 0016; whether the device reports the buttons this way is unverified): D-pad is up/down/left/right, east (right face button) is A, south (bottom) is B, L1 is L, R1 is R, Start and Select as labelled. X, Y, L2, R2, the sticks and Menu do nothing. **Exit: hold Select + Start + L1 + R1 together.** Menu belongs to the launcher.

To change the mapping copy `PicoTracker.pak/share/input.ini.example` to `.userdata/tg5040/PicoTracker/input.ini` (or pass `--input-profile PATH`). One `advance_button = trimui_control` per line, `#` or `;` comments on their own lines:

```
up = dpad_up
down = dpad_down
left = dpad_left
right = dpad_right
a = east
b = south
l = l1
r = r1
start = start
select = select
left_stick_as_dpad = false
```

These are the defaults. Advance buttons: `up down left right a b l r start select`. Controls: `dpad_up dpad_down dpad_left dpad_right east south west north l1 r1 l2 r2 start select menu l3 r3`, or `none` to unbind. `left_stick_as_dpad` accepts `true/false/1/0/yes/no/on/off`. A bad line is skipped and logged with its line number; a missing file means the defaults. The exit chord cannot be changed. To find the real button indices, add `--log-input` to the `exec` line in `launch.sh`: the log then lists the joystick name, counts and every press.

## Checklist

1. **Get the binary.** Either install the CI zip as above and go straight to step 9 for the app (the smoke steps 3 to 8 need the local build), or build locally on a machine with Docker: `scripts/build-trimui.sh`. It writes `build/trimui/picotracker_desktop` and prints the glibc versions and shared libraries it needs. Expect `GLIBC_2.17` only. The smoke pak is only installed by `scripts/deploy-trimui.sh`.
2. **Record the device facts** before installing anything, from a shell on the device (ssh or a terminal tool) if you can get one: `uname -a`, `ls -l /lib/libc.so.6`, `ls /usr/lib | grep -i -E 'SDL|drm|gbm|asound'`, `cat /proc/meminfo | head -3`. Put them in the results table below. If you cannot get a shell, write "no shell" and carry on.
3. **Preview the install:** `scripts/deploy-trimui.sh --dry-run --sd-root /Volumes/<CARD>` (or `--ssh root@<ip>`). Read the printed actions. Nothing is changed.
4. **Install.** Pick one:
   - SD card in your computer: `scripts/deploy-trimui.sh --sd-root /Volumes/<CARD>`, then eject the card and put it back in the device.
   - ssh (the device is reported to accept ssh as root on port 22; `deploy-trimui.sh --ssh` itself has not been run against it): `scripts/deploy-trimui.sh --ssh root@<ip>` or `--ssh root@<ip>:<SD root on the device>` (default SD root `/mnt/SDCARD`).

   This creates `Tools/tg5040/PicoTracker.pak/` (the app) and `Tools/tg5040/PicoTrackerSmoke.pak/` (boot smoke). Nothing outside `Tools/` is touched.
5. **Boot the smoke on the device.** In NextUI open Tools and start `PicoTrackerSmoke`. The screen may flash or go black for a few seconds, then return to the launcher. It runs `picotracker --smoke` once and exits.
6. **Collect the result.** SD card: put the card in your computer and run `tests/trimui/smoke.sh --sd-root /Volumes/<CARD>`. ssh: `tests/trimui/smoke.sh --ssh root@<ip>` runs the smoke itself and copies the result back. Files go to `build/trimui-smoke/<timestamp>/` (`smoke.log`, `smoke.bmp`, `RESULT`). The script prints `PASS` only if the exit status was 0 and the screenshot is not empty.
7. **If it failed:** read `smoke.log`. A missing `RESULT` means the app never finished. A loader error (`GLIBC_x not found`, `libSDL2... not found`) means ADR 0015's assumptions are wrong for this device. An SDL error (`No available video device`) means a driver must be chosen: add `export SDL_VIDEODRIVER=...` and `SDL_AUDIODRIVER=...` to both `launch.sh` templates in `scripts/deploy-trimui.sh`, redeploy and repeat from step 5. Candidates are `kmsdrm` or `fbdev` for video. For audio leave it unset or use `dsp`; `alsa` is rejected by the static SDL build (ADR 0018). Record what you tried.
8. **Open `smoke.bmp`.** It should show the picoTracker project screen. Note whether it is complete and not black or garbled.
9. **Start the real app:** open Tools, start `PicoTracker`. Check what you see and hear. Does it draw, is it centred with black side borders (ADR 0016 says 280 px each side), does sound play, which buttons move the cursor? The launcher passes `--display trimui`. Quit with the chord Select + Start + L1 + R1 and note whether it returns to NextUI. If buttons do nothing or are mixed up, edit `input.ini` (see above); to see the indices, add `--log-input` to `launch.sh` and read the log.
10. **Record the results** in the table below and commit the edit (or send it back to the board). Attach `smoke.log` and `smoke.bmp` from `build/trimui-smoke/`, and `.userdata/tg5040/PicoTracker/logs/picotracker.log` from the card after step 9.

## Where logs and files are

| What | Where on the SD card |
| --- | --- |
| Smoke log, screenshot, exit status | `.userdata/tg5040/PicoTracker/smoke/{smoke.log,smoke.bmp,RESULT}` |
| App log (previous run: `.1`) | `.userdata/tg5040/PicoTracker/logs/picotracker.log` |
| Projects and samples | `.userdata/tg5040/PicoTracker/{projects,samples}/` |
| Installed paks | `Tools/tg5040/PicoTracker.pak/`, `Tools/tg5040/PicoTrackerSmoke.pak/` |

Uninstall: delete both pak folders. Delete `.userdata/tg5040/PicoTracker/` too to remove user data.

## Results (fill in)

Copy this table per device and attempt.

| Item | Result |
| --- | --- |
| Date, tester | |
| NextUI version | |
| Device glibc (`/lib/libc.so.6`) and kernel | |
| Device ships `libSDL2`, `libdrm`, `libgbm`, `libasound`? | |
| Transport used (SD card / ssh) | |
| Smoke: `RESULT` value and PASS/FAIL | |
| Smoke: screenshot shows the project screen? | |
| SDL video and audio drivers that worked | |
| Real app: draws, borders, sound | |
| Real app: buttons that worked, Menu button behaviour | |
| Free RAM with the app running (if measurable) | |
| Problems and follow-ups | |

## Open points this run should settle

These are the ADR 0017 and ADR 0015 "unverified" items: the `launch.sh` contract and environment variables of the installed NextUI, whether `launch.sh` runs without the executable bit on FAT, the working SDL drivers, glibc and kernel versions, `/bin/sh` behaviour, and 48 MiB sample memory next to the launcher. Also unverified: that `js0` and 'TRIMUI Player1' deliver the ADR 0016 default indices, that the static SDL's `dlopen` of `libdrm`/`libgbm` works with the device libraries, KMS/DRM versus fbdev output, and audio through the `dsp` or ALSA path. The packaged `launch.sh` seeds `share/default-data` and passes `--display trimui` but not `--log-input`; the smoke pak's `launch.sh` (from `scripts/deploy-trimui.sh`) runs `--smoke` without `--display trimui`.

## Recording on the device

Measured on 2026-11-17 (NextUI 20261013-0); details in [planning/findings/trimui-recording.md](../planning/findings/trimui-recording.md).

- **Which input works:** the built-in mic. The Smart Pro has one mic on MIC1 (left channel), so recording is mono. In the pak, source Mic gave a mono 44.1 kHz 16-bit file with signal in two runs. Line In (the firmware's default source) is stereo with only noise on the right; the launcher sets `PICOTRACKER_LINEIN_MONO=1` to copy L to R, but that was added after the device runs and is **not re-checked on the device**.
- **Select the source:** on the Record screen change the source to Mic. In one try Edit+Right did not change it on the device (the check writes `RECORDSOURCE` into `.config.xml` instead), so if it will not change, treat it as unresolved. The app does not touch the ALSA mixer: input gain is whatever the firmware left (`MIC1/MIC2 gain volume`, `ADC volume`, `ADCL Input MIC1 Boost`; read with `amixer -c 0`). `launch.sh` saves and restores these around the run. The firmware's own threshold setting (10 of 255) still decides when a level-triggered take starts.
- **Where recordings go:** `.userdata/tg5040/PicoTracker/recordings/REC01.wav`, then `REC02.wav`, and so on.
- **Load one in the sampler:** open the sampler (instrument sample selection) and pick the file from the browser; the check's log shows the sample editor loading `REC01.wav`.
- **Sample memory:** 384 MiB by default (`--sample-memory-mib 384` in `packaging/trimui/launch.sh`, measured in [trimui-sample-memory.md](../planning/findings/trimui-sample-memory.md)). To change it edit that number in the pak's `launch.sh` on the card; a sample that does not fit in what is left fails to load.
- **Check it yourself:** `tests/trimui/record_check.sh --ssh root@<ip> --source mic --seconds 5 [--label NAME]` deploys the pak, records, and checks the WAV (16-bit, 44.1 kHz, length, signal, mono or L equal to R), the mixer before and after, and the log for underruns while recording. `tests/trimui/record_check.sh --emulated` runs the container check. Use `--source line` for Line In. Results go to `build/trimui-record/`.
- **Still unverified:** a headset on the 3.5 mm jack (which pin feeds MIC2, and whether it lands left), the real buttons (the check uses a virtual pad), Line In with the mono flag, changing the source in the UI, and capture overruns (not logged by the app).
