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

Run 2026-11-17 after the board was power-cycled (`device_acceptance.sh --minutes 5`, 5 failures):

| item | result | detail |
|---|---|---|
| wedge | PASS | no process stuck in the framebuffer driver |
| long-run | FAIL | `video driver: mali`, 1280x720, panel offset 280,0, but the app was gone at 110 s (exit status 0, no error in the log, last line `MALI_CreateWindow done`) |
| audio | PASS | 0 underrun/overrun/xrun lines in the long-run log |
| memory (long run) | PASS | peak RSS 23 MB, lowest MemAvailable 738 MB, swap 0, no OOM |
| memory-384 probe | FAIL by script, numbers fine | 48 loaded / 2 refused, RSS 404 MB, MemAvailable 335 MB, swap 0, OOM 0, audio errors 0, alive |
| chord-exit | PASS | launch.sh exit 0, NextUI processes unchanged |
| relaunch | PASS | second launch ran, exit 0 |
| mixer | FAIL | `DAC volume` 160,160 before and 0,0 after; a `Soft Volume Master` control (255,255) appeared |
| display | FAIL | the captured panel shows NextUI's "Native SSH" screen, not the tracker: the picture checks (content region, black borders) fail on that image; no app log or process listing was collected |
| record | FAIL on mixer only | WAV 44100 Hz mono 5.56 s with signal, no xruns while capturing; the same DAC volume 160 to 0 difference |

Second run, same day, with the device at the NextUI home screen (6 failures):
PASS wedge, audio, record, relaunch. FAIL long-run (app gone at 10 s, exit 0, no error, log ends at
`MALI_CreateWindow done`), memory (no samples because of that), mixer, chord-exit, and display. Display: the
capture shows NextUI's home menu ("Recently Played / Game Boy Advance / Tools"), not the tracker, although the
app reported `video driver: mali`, 1280x720, ran at 32% of a core and exited 0 on the chord. memory-384: 48
loaded / 2 refused, RSS 407 MB, MemAvailable 335 MB, no OOM, no swap, but 2 audio errors this time.
Reading: an app started over SSH while NextUI is running does not get the panel; NextUI keeps drawing
fb0. The early exits (110 s, then 10 s, status 0) come from an SDL quit, Escape or the chord, none of which the
script sends before the 5 min; the source is unknown.

Open points from the first run, not yet resolved:

- The display check ran while the device was sitting on NextUI's Native SSH settings screen, so it did not
  test the tracker's picture. It needs a re-run with NextUI at its home screen.
- The 110 s early exit of the long run is unexplained (exit 0, so not a crash); it may be the same
  foreground-app interference, or a real exit in the app.
- `DAC volume` changing 160 to 0 is not one of the controls launch.sh saves and restores. Whether the app,
  NextUI's audio monitor or the SSH screen changed it is not established; `Soft Volume Master` is most likely
  created by ALSA the first time the device is opened.

## Hand-over checklist (needs a person)

1. Power-cycle the Trimui (clears the wedged fb0), then run `tests/trimui/device_acceptance.sh`.
2. Look at the panel during the run: the tracker UI is drawn, centred 720x720, no tearing or flicker.
3. Listen: playback is clean (no clicks or dropouts), and a recording made with the mic sounds right.
4. Press real buttons: navigation, Play/Stop, and the exit chord (Select+Start+L1+R1) return to NextUI.
5. After exit, NextUI menus respond and volume is as before.

## Start-up ALSA underrun (C-58)

A person-launched run (`build/trimui-observe/20261117-215557`) logged one `ALSA lib pcm.c:8549 snd_pcm_recover
underrun occurred` during init, before the window was created, and none afterwards. Decision: the acceptance
audio check now counts underrun/overrun/xrun lines only after the `video driver:` line; start-up lines are
printed as a note. No driver change: `SDLAudioDriver` opens the device paused and only unpauses in
`StartDriver`, so the start-up line cannot be attributed to playback from the code alone, and it could not be
reproduced here (no device access). If it recurs after the window exists, it is counted and fails the check.

## Person-launched run (Q-11) and findings from the observer

Decision on Q-11: the display, long-run and chord items are done by a person who starts the pak from NextUI's
menu; `tests/trimui/device_observe.sh` only collects the numbers (mixer and NextUI processes before/after,
RSS/MemAvailable/swap every 15 s, OOM lines, `video driver:` line, xrun count). It was run once against the
device with nobody launching: it caught a `picotracker` that was not started by this card (a 45 s run, log
`display: trimui panel 1024x768`, no `video driver:` line, device-side `PicoTrackerSmoke.pak` created at 14:23 and its
log written at 14:25). So the device is shared: another job started and killed picotracker while the acceptance runs
were going, which fits the unexplained 10 s and 110 s exits of the SSH-launched runs. In that observed run the mixer
`DAC volume` went from 0,0 (before) to 160,160 (after), i.e. NextUI sets it back, so the 160 to 0 change in the first
run is not caused by the app being left in a bad state.

### Observer run after Q-12 (device lost)

`device_observe.sh` caught a person-launched `picotracker` (pid 8924, RSS 20 MB, MemAvailable 760-780 MB, swap 0) and
sampled it for 75 s; then the samples stopped and the device no longer answered on port 22 (connect timed out, still
unreachable minutes later). So within about 80 s of a run the Trimui dropped off the network: the app hanging or the
device sleeping, powering off or losing Wi-Fi are all possible and nothing here separates them. No app log, mixer
or OOM numbers were collected for this run. Needs a person at the device: what the panel shows, whether it is on,
whether the app is still running, and the device's Wi-Fi/sleep settings.

### Observer run 3 (person-launched, after Q-13)

Device reachable again (uptime 4:39, so it had not rebooted; the earlier loss looks like a network drop, not a crash).
`device_observe.sh` now uses SSH keep-alives so a lost device fails the script instead of hanging it.
Person-launched run, 45 s, left with the exit chord:

| item | result |
|---|---|
| video | PASS, `video driver: mali` |
| audio | FAIL by the strict count: 1 line, `ALSA lib pcm.c:8549:(snd_pcm_recover) underrun occurred`, logged during start-up before the window was created; none during the rest of the run |
| memory | peak RSS 23 MB, lowest MemAvailable 751 MB, swap 0, no new OOM line |
| mixer | PASS, identical before and after (so the earlier DAC volume change came from NextUI/other jobs, not the app) |
| NextUI processes | PASS, unchanged |

Not verified by anyone yet: that the tracker UI is on the panel centred 720x720 without tearing (the observer cannot
see the panel), the 5 minute duration (runs so far were 45 s to 110 s because a person ended them or another job
interfered), and listening quality. The memory-384 probe and record check are in the earlier runs above.
