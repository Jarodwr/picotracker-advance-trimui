# Trimui Smart Pro: sample memory budget

ADR 0017 shipped `--sample-memory-mib 48` in the pak's `launch.sh` as an estimate. This measures it on a device and replaces it. **Result: 384 MiB.**

## Method

Device: Trimui Smart Pro, TinaLinux 4.9.191 aarch64, glibc 2.33, 997,792 kB RAM (`MemTotal`), no swap device, NextUI 20261013-0 running (its launcher process stayed resident during every run). Binary: `scripts/build-trimui.sh` output, installed with the packaged `launch.sh` layout under `Tools/tg5040/PicoTracker.pak`.

[`tests/trimui/memory_probe.sh`](../../tests/trimui/memory_probe.sh) drives it over ssh. For each budget it writes 8 MiB mono 16-bit 44.1 kHz WAVs under `.userdata/tg5040/PicoTracker/samples/memprobe/` (removed afterwards), starts `picotracker --display trimui --sample-memory-mib <budget>` with `budget/8 + 2` `--sample-probe` arguments (two more files than fit), waits for every probe line, lets the app idle for 8 s with its audio device open, then reads `MemAvailable`, `VmRSS` and `VmHWM` from `/proc/<pid>/status`, swap use, new OOM lines in `dmesg` and audio error lines in the app log. Page cache is dropped before each run so `MemAvailable` before the run is comparable.

## Results

| Budget MiB | Samples loaded / refused | MemAvailable before | MemAvailable with app | App RSS | Peak RSS | Swap | OOM kills | Audio errors |
|---|---|---|---|---|---|---|---|---|
| 48 | 6 / 2 | 799 MB | 736 MB | 67 MB | 67 MB | 0 | 0 | 0 |
| 128 | 16 / 2 | 799 MB | 656 MB | 147 MB | 147 MB | 0 | 0 | 0 |
| 256 | 32 / 2 | 799 MB | 527 MB | 275 MB | 275 MB | 0 | 0 | 0 |
| 384 | 48 / 2 | 799 MB | 398 MB | 404 MB | 404 MB | 0 | 0 | 0 |

(MB here is 1024 kB; raw kB values are in the probe's TSV output.) At every budget the pool filled exactly (`remaining=0`) and the two extra files were refused with `index=-1`, so the budget is enforced on the device as ADR 0010 describes. The app's RSS is the budget plus about 19-20 MB of fixed overhead, and peak RSS equals final RSS, so loading does not overshoot the budget transiently. `MemFree` at 384 MiB drops to about 30 MB, but that is reclaimable cache; `MemAvailable` is the figure that matters and stays near 400 MB.

## Decision

The rule was the largest budget that leaves at least 200 MB `MemAvailable` with no OOM. All four tested budgets pass; the largest, 384 MiB, leaves 398 MB. `launch.sh` now passes `--sample-memory-mib 384`, and the pak `README.txt` tells the user that samples share this budget and a sample that does not fit in what is left fails to load. Only four budgets were measured; extrapolating the linear trend, 512 MiB would leave roughly 270 MB, but it was not run, so it is not claimed.

## Limits of this evidence

- Audio underruns are not measured. The app logs no underrun counter, and the probe runs with the audio device open but no song playing, so the zero in the "Audio errors" column means only "no audio error lines in the log". Underruns under load need a playing song and a counter in the app (or `/proc/asound` status), which is a separate piece of work.
- The 8 MiB test samples are zero-filled. The pool stores converted 16-bit PCM either way, so memory use does not depend on content.
- Measured with NextUI's launcher resident and the app drawing to the same framebuffer. Memory taken by a real project (song data, view buffers) is small next to the budget and was not separated out.
- Loading 384 MiB of WAVs from the SD card takes a while; the probe waits up to 300 s for the probes to finish.
