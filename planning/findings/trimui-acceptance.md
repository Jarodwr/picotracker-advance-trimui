# Trimui device acceptance (C-45)

Status: **BLOCKED, not run.** The device's `/dev/fb0` is wedged, so no acceptance item could be measured.
No numbers are recorded below because none exist yet.

## What was found (2026-11-17, device up 2:54, load average ~8)

Five processes were stuck in the framebuffer driver (uninterruptible, `wchan` read from `/proc/<pid>/wchan`):

| pid | wchan | process |
|---|---|---|
| 16965 | lock_fb_info | fbprobe |
| 17658 | fb_open | c39_pt |
| 17773 | fb_open | fbset |
| 18425 | fb_open | dd |
| 18938 | fb_open | picotracker (PicoTrackerSmoke.pak) |

These are leftovers from earlier fb0 probes (C-37, C-43) and a smoke run. This is the wedge described in
`tests/trimui/display_check.sh`; only a reboot clears it, and the card rules forbid rebooting. The deployed
pak is present (`bin/picotracker`, 4.3 MB, launcher with the vendor-SDL display path), so the acceptance can
run as soon as the board is power-cycled.

## What was delivered

`tests/trimui/device_acceptance.sh` (wraps `display_check.sh`, `record_check.sh`, `memory_probe.sh`
unedited, deploys nothing). It first checks for a wedged fb0 and exits 2 (BLOCKED) with the stuck pids;
this was run against the device and printed `FAIL wedge`. With a healthy fb0 it prints PASS or FAIL for:
wedge, long-run (5 min, real `video driver:` line), audio (no underrun/overrun/xrun), memory (RSS, peak RSS,
MemAvailable sampled every 15 s, no OOM in dmesg, no swap), mixer (identical before/after), chord-exit
(status 0, NextUI processes unchanged), relaunch (second launch exits 0), display, record, memory-384.
Raw output goes to `build/trimui-acceptance/<timestamp>/`.

Not run on the device: the long-run, relaunch and memory sections of the script, so their device-side shell
is untested beyond `bash -n` and shellcheck.

## Numbers

Pending a power cycle: run `tests/trimui/device_acceptance.sh` and paste the peak RSS, lowest MemAvailable,
audio xrun count, exit statuses and probe row here.

## Hand-over checklist (needs a person)

1. Power-cycle the Trimui (clears the wedged fb0), then run `tests/trimui/device_acceptance.sh`.
2. Look at the panel during the run: the tracker UI is drawn, centred 720x720, no tearing or flicker.
3. Listen: playback is clean (no clicks or dropouts), and a recording made with the mic sounds right.
4. Press real buttons: navigation, Play/Stop, and the exit chord (Select+Start+L1+R1) return to NextUI.
5. After exit, NextUI menus respond and volume is as before.
