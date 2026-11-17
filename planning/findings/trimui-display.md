# Trimui display check on the physical panel (C-43)

**Result: not proven. The picture could not be captured because `/dev/fb0` on the device is wedged in the kernel. `tests/trimui/display_check.sh` is written and tested on the host, and fails on the device with the reason below.**

Run on 2026-11-17 against the Trimui Smart Pro (TinaLinux 4.9.191 aarch64, NextUI 20261013-0), binary built from `main` at 7acae46 with `scripts/build-trimui.sh` (vendor-SDL dynamic link), packaged with `scripts/package-trimui.sh` and deployed with `scripts/deploy-trimui.sh --ssh` (the pak on the card is that build).

## What happened

1. The device's framebuffer, from sysfs (plain file reads, safe): 32 bpp, stride 5120 (1280 x 4), virtual size 1280x13107, mode `U:1280x720p-63`, pan `0,720` (the visible page starts at row 720, so a capture is `dd bs=5120 skip=720 count=720`). The panel is 1280x720 landscape, as ADR 0016 assumed.
2. Every `open()` of `/dev/fb0` blocks forever in the kernel. `/proc/<pid>/stack` of a blocked opener:

   ```
   fb_open+0x9c  <- chrdev_open <- do_dentry_open <- vfs_open <- path_openat <- do_sys_open
   ```

   The holder is `/tmp/fbprobe panloop` (pid 16965, left by the C-37 display probe), in D state inside `lock_fb_info` from `do_fb_ioctl`: it holds the fb0 info lock and never returns. It and everything queued behind it are uninterruptible; `kill -9` does nothing. Only a reboot clears them, and the card forbids rebooting.
3. Processes stuck on fb0 when this card ran (`ps` state D, `/proc/<pid>/wchan`): `fbprobe` (C-37's), `c39_pt` (a `--smoke` run from C-39's session), plus three this card added before the cause was clear: `fbset` (a read of the mode), a `dd` of fb0 (the first capture attempt) and the deployed `picotracker --smoke`. The last one is the useful result: **the vendor SDL 2.30.8 build of the app opens `/dev/fb0` during `SDL_Init`, so while fb0 is wedged the app cannot start at all** (no `video driver:` line, no log). The wedge is therefore a blocker for every on-device run, not only for the capture.
4. The vendor screenshot tool the C-37 report found, `/usr/trimui/bin/fbscreencap`, is an `fbgrab` build (usage `[-hi] [-C vt] [-d dev] [-s n] [-z n] filename.png`); it opens fb0 and issues `FBIOGET_VSCREENINFO`, so it hangs the same way. It was not run.

## What was checked and what was not

| Check | Status |
| --- | --- |
| Picture not black, 720x720 centred with 280 px borders, matches the `--smoke` screenshot | **Not run on the device** (no capture). The checker is exercised on a synthetic 1280x720 page: a good page passes, a black page fails not-black and content, a mis-centred picture fails the borders, and a different picture fails the smoke comparison. |
| App log has a real `video driver:` and the real panel size | **Not run** (the app hangs in `SDL_Init` before it logs). |
| CPU use of the picotracker process | **Not run.** |
| Exit chord returns the process cleanly, NextUI still usable | **Not run.** NextUI itself was untouched and responsive over ssh throughout (`MinUI.pak/launch.sh` still running). |
| App not running at the end | Yes, nothing of ours runs; the stuck `picotracker --smoke` (pid 18938) and `dd` (18425) are still in D state until the next reboot. They hold no CPU. Injector and helper files under `/tmp/picotracker-display-check` were removed; the card was written only under `Tools/tg5040/PicoTracker*.pak` and the data root. |

## Making the check usable

`tests/trimui/display_check.sh --ssh root@HOST` (needs `TRIMUI_SSH_PASSWORD` or a key, python3, docker for the injector on first run; `PICOTRACKER_UPSTREAM_ROOT` when the worktree has no `upstream/` submodule) does, on the device: a wedge gate (any process in `fb_open`/`lock_fb_info` makes it stop before starting anything, which is what the run recorded in `build/trimui-display/run-wedged/`), the smoke run for the reference picture, `launch.sh` in the background, CPU from `/proc/<pid>/stat` over 5 s, a background `dd` of the visible page, the exit chord via `uinput_inject`, and before/after process lists of NextUI. `tests/trimui/display_check.py` converts the page to `panel.bmp` and does the picture checks (perceptual: 24x24 block means of the content region against the smoke screenshot, mean difference at most 24 and at most 10% of blocks off by more than 48).

Next step: reboot the device (a person; the card forbids it), then run `tests/trimui/display_check.sh --ssh root@$TRIMUI_SSH_HOST` and attach `panel.bmp`. Two cautions for anyone probing fb0 afterwards: do not run `fbset`, `fbgrab`/`fbscreencap` or a probe that issues fb ioctls in a loop (`fbprobe panloop` is what wedged it, most likely the pan ioctl), and always background fb0 readers. If the wedge comes back without a probe running, the vendor driver itself is at fault and the app needs to avoid fb0 (for example `SDL_VIDEODRIVER` set to a non-fbdev driver) which is a `platform/**` question for a later card.
