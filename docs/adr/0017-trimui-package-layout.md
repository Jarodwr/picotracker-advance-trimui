# ADR 0017: Trimui Smart Pro package layout, launch script and data root

Status: Accepted (2026-11-17), amended after implementation (`scripts/package-trimui.sh`, `packaging/trimui/`, CI). Device-only items stay under "Unverified until run on the device".

## Context

ADR 0008 maps firmware paths such as `/projects` beneath one host data root, chosen with `--data-dir`. ADR 0016 defines Trimui input and display. This ADR defines how the port is packaged on the SD card so a packaging card can build it.

Facts come from the C-14 research report. Verified from NextUI `PAKS.md`: a tool is a folder `<Name>.pak` containing `launch.sh`, placed at `Tools/<platform>/` on the SD root (`<platform>` is `tg5040` for the Smart Pro). `.system/` is wiped on every NextUI update. `.userdata/<platform>/` holds per-platform config and logs. The launcher provides `USERDATA_PATH`, `LOGS_PATH`, `SAVES_PATH`, `BIOS_PATH`, `HOME` and `DEVICE`. Everything else (glibc version, which SDL2 the device ships and its video and audio backends, the stock and CrossMix launch contracts) is **unverified** and listed at the end.

## Decision

### 1. Target firmware

Primary target is **NextUI** on the Smart Pro (`tg5040`, aarch64, glibc). Stock TrimUI OS and CrossMix-OS are a second-pass check, not a promise. One aarch64 binary and one pak folder serve all of them. The Smart Pro S (`tg5050`, A523) is out of scope.

### 2. Folder tree

The release artifact is a zip that is extracted at the SD card root. It contains only the pak folder:

```
Tools/
  tg5040/
    PicoTracker.pak/
      launch.sh               # launcher entry point, executable (mode 0755)
      bin/
        picotracker           # aarch64 host binary (the desktop wrapper built for the trimui backend)
      lib/                    # only if the device lacks a usable SDL2 (section 4); otherwise absent
        libSDL2-2.0.so.0
      share/
        default-data/         # seed content copied once into the data root (section 5)
          projects/           # may be empty; kept with a .keep file
          samples/            # optional starter samples
        input.ini.example     # documented defaults from ADR 0016
      licenses/
        NOTICE.md             # index: component, version, license, file
        picotracker-BSD-3-Clause.txt     # upstream/picoTracker/LICENSE
        Ubuntu-Font-Licence-1.0.txt
        SDL2-zlib.txt         # if SDL2 is shipped or statically linked
        <one file per other component actually linked or bundled>
      README.txt              # install, update and exit instructions (3 paragraphs)
```

Nothing is written under `PicoTracker.pak/` at runtime, so the pak can be deleted and replaced wholesale. `pak.json` is not included; add it only if the device test shows NextUI needs it.

User data lives outside the pak:

```
.userdata/
  tg5040/
    PicoTracker/              # the host data root (--data-dir)
      projects/               # firmware /projects
      samples/                # firmware sample directory
      input.ini               # optional, ADR 0016 section 4
      logs/
        picotracker.log       # current run
        picotracker.log.1     # previous run
```

If `$USERDATA_PATH` is not provided (other firmware), the script falls back to `<pak>/../../../.userdata/tg5040`, which is the same location relative to the SD root.

### 3. Launch script

`launch.sh` (POSIX `sh`, no bashisms, since the device shell is probably busybox ash):

```sh
#!/bin/sh
# PicoTracker launcher for Trimui Smart Pro (tg5040)
PAK_DIR="$(cd "$(dirname "$0")" && pwd)"
SD_ROOT="$(cd "$PAK_DIR/../../.." && pwd)"

USERDATA="${USERDATA_PATH:-$SD_ROOT/.userdata/tg5040}"
DATA_DIR="$USERDATA/PicoTracker"
LOG_DIR="$DATA_DIR/logs"

mkdir -p "$DATA_DIR/projects" "$DATA_DIR/samples" "$LOG_DIR"

# First run only: seed starter content. Never overwrites existing files.
if [ ! -e "$DATA_DIR/.seeded" ]; then
    cp -rn "$PAK_DIR/share/default-data/." "$DATA_DIR/" 2>/dev/null
    : > "$DATA_DIR/.seeded"
fi

# Keep the previous log
[ -f "$LOG_DIR/picotracker.log" ] && mv -f "$LOG_DIR/picotracker.log" "$LOG_DIR/picotracker.log.1"

# SDL: software rendering, SDL-selected drivers unless the device test says otherwise
export SDL_RENDER_DRIVER=software
export SDL_NOMOUSE=1
# SDL_VIDEODRIVER / SDL_AUDIODRIVER are left unset so SDL chooses; after the device test
# add e.g. export SDL_VIDEODRIVER=kmsdrm (or fbdev) here. Leave SDL_AUDIODRIVER unset (SDL picks dsp); alsa is rejected by the static SDL build (ADR 0018).

# Libraries: bundled lib/ first, only if present
if [ -d "$PAK_DIR/lib" ]; then
    export LD_LIBRARY_PATH="$PAK_DIR/lib:${LD_LIBRARY_PATH:-}"
fi

export HOME="${HOME:-$USERDATA}"
cd "$PAK_DIR" || exit 1

# Sample memory is lower than the desktop default (128 MiB): 1 GB RAM is shared with the launcher.
exec "$PAK_DIR/bin/picotracker" \
    --display trimui \
    --data-dir "$DATA_DIR" \
    --sample-memory-mib 48 \
    --log-input \
    >"$LOG_DIR/picotracker.log" 2>&1
```

Notes:

- `exec` replaces the shell, so the launcher sees the app's exit status. Exit uses the chord from ADR 0016 (Select+Start+L1+R1), which sets `quit_` and returns to the launcher.
- `--display trimui` is the switch defined in ADR 0016 section 5; `--log-input` stays on until the input profile is verified, then becomes opt-in.
- The SDL driver variables are deliberately left unset (SDL chooses) until the device test names the working backend; the script is written so that exporting `SDL_VIDEODRIVER` and `SDL_AUDIODRIVER` before launch (for testing over a shell) overrides them. Once verified, replace the empty defaults with the verified values (candidates: `kmsdrm` or `fbdev` for video). For audio, do not export `alsa`: the device's static SDL build reports it "not available". Leave `SDL_AUDIODRIVER` unset or use `dsp` (ADR 0018).
- `SDL_RENDER_DRIVER=software` matches ADR 0016: the GPU is PowerVR GE8300 and GL/GLES is not assumed.
- 48 MiB is an estimate, not a measurement. It can be changed by editing the script; `--sample-memory-mib` already exists.

### 4. SDL2 and library paths

Decision: **prefer linking SDL2 statically** into `bin/picotracker`, built with the video and audio backends the device needs, so the pak does not depend on the unknown device library. If static linking proves impossible (backend needs dlopen of vendor libraries), ship `lib/libSDL2-2.0.so.0` and rely on `LD_LIBRARY_PATH` as in the script. Either way the toolchain sysroot must be old glibc (the Linaro GCC 7.5 sysroot used by the community toolchains, glibc around 2.25), so the binary does not require symbols newer than the device. The C++ standard of the upstream firmware against GCC 7.5 is a packaging-card question, not decided here.

The binary's `rpath` is not set; `LD_LIBRARY_PATH` is. The pak never touches system library directories.

### 5. Data root and samples

- The data root is `$USERDATA_PATH/PicoTracker` (default `/<SD>/.userdata/tg5040/PicoTracker`), passed with `--data-dir`. This extends ADR 0008 without code change: `HostFileSystem` already resolves every firmware path beneath it. FAT32 has no symlinks, so the symlink rejection is harmless there.
- Projects are `projects/` and samples are `samples/` in the data root, exactly the firmware's `/projects` and sample directory. Users add samples by placing files in `.userdata/tg5040/PicoTracker/samples/` from a PC.
- `.userdata/` is not wiped by NextUI updates, unlike `.system/`. The pak must never use `.system/` for anything.
- Seeding happens once (marker `.seeded`) and uses `cp -n`, so it cannot overwrite user files.

### 6. Bundled notices

Per `planning/findings/license-review.md`, the BSD 3-Clause upstream license requires binary distributions to reproduce the notice and disclaimer in accompanying materials. The pak therefore carries `licenses/` with:

- the upstream root `LICENSE`;
- the licence file of every component that is actually linked or bundled in the Trimui binary (generated at packaging time from the real link set, not copied from the upstream list: the upstream list has label typos for TinyXML2 and yxml, so use the referenced licence files);
- the Ubuntu Font Licence 1.0 for the display font;
- SDL2 (zlib) when SDL2 is statically linked or shipped;
- `NOTICE.md`, an index table of component, version, licence and file.

The packaging step fails if `licenses/NOTICE.md` lists a file that does not exist. Copyright holders' names are not used for endorsement in the README or the zip name. The review is a source inventory, not legal advice.

### 7. Install and update

- **Install:** extract the zip at the SD card root (the zip's top-level directory is `Tools/`), eject, and start `PicoTracker` from NextUI's Tools list. The first run creates the data root.
- **Update:** extract the new zip over the SD root. Because the zip only contains `Tools/tg5040/PicoTracker.pak/`, projects and samples in `.userdata/` are untouched. Stale files from an older pak are the only hazard, so the update instructions say to delete `Tools/tg5040/PicoTracker.pak` first (safe, because the pak holds no user data), then extract.
- **Uninstall:** delete the pak. To remove user data as well, delete `.userdata/tg5040/PicoTracker/`.
- **Backup:** copy `.userdata/tg5040/PicoTracker/` from a PC.
- Project format compatibility across versions is the firmware's, not the package's, and is not addressed here.

## Amendments from the implementation

- **Target:** the first release supports NextUI only (board decision Q-2). Stock TrimUI OS and CrossMix-OS are not supported or tested.
- **Artifact:** `scripts/package-trimui.sh` builds `PicoTracker-trimui-tg5040-<version>.zip`, byte-reproducible for the same inputs (`SOURCE_DATE_EPOCH`), and the binary is stripped. CI (`.github/workflows/linux-arm64.yml`, job `trimui-cross`) builds it on every push and uploads it as the artifact `PicoTracker-trimui-tg5040-<short commit>`; for `v*` tags it is also attached to the GitHub release.
- **Contents differ from section 2:** no `lib/` (SDL2 is static, ADR 0015); `licenses/` is generated from `packaging/trimui/licenses/components.tsv` and the packaging step fails if `NOTICE.md` lists a missing file; `share/default-data/{projects,samples}` is seeded once; `README.txt` is included. `pak.json` is not included.
- **`launch.sh`** is `packaging/trimui/launch.sh`, not the sketch in section 3: it does **not** pass `--log-input` (that is opt-in now), and `SDL_VIDEODRIVER`/`SDL_AUDIODRIVER` are unset so SDL chooses. `--display trimui`, `--data-dir`, `--sample-memory-mib 48` and the `.seeded` marker are as in section 3.
- **Exit** is the chord from ADR 0016; Menu is not used.
- **USB MIDI** is out of scope for this board (Q-3: deferred to a later board).
- **Update** is as section 7: delete the pak folder, then extract the new zip at the SD root.

## Consequences

- A packaging card can implement the zip as: build `bin/picotracker`, copy `launch.sh`, `share/`, generated `licenses/`, `README.txt`.
- User data survives updates and NextUI system updates because it never lives inside the pak or `.system/`.
- The script depends on NextUI environment variables but has a fallback path, so it also works if launched by hand.
- `--display trimui`, `--log-input`, `--input-profile`, `--data-dir`, `--smoke` and `--sample-memory-mib` all exist in `platform/desktop/main.cpp`.

## Unverified until run on the device

1. The `Tools/tg5040/Name.pak/launch.sh` contract in the installed NextUI version, including working directory, environment variables (`USERDATA_PATH` etc.), whether `pak.json` is needed, and how exit returns to the launcher.
2. Whether stock TrimUI OS and CrossMix-OS can launch the same pak, and their folder names.
3. The working SDL video and audio drivers (`kmsdrm`, `fbdev`; audio is `dsp`, since `alsa` is rejected, see ADR 0018). Reported for the device (NextUI 20261013-0): no system `libSDL2`, `libdrm` or `libgbm`, only `libasound.so.2`; `/dev/fb0` and `/dev/dri/card0` exist. Whether the static SDL's `dlopen` of `libdrm`/`libgbm` works with that is unverified; the app has not been run on the device.
4. glibc and kernel versions: reported as glibc 2.33, kernel 4.9.191 (the binary needs `GLIBC_2.17` only). The GCC 7.5 question was settled by ADR 0014: it cannot build the firmware.
5. That `/bin/sh` is busybox ash and that `cp -rn`, `mkdir -p`, `cd`, and `exec` behave as used.
6. That the SD card is FAT32 or exFAT with the expected case and permission behaviour, including `launch.sh` being executable (FAT has no mode bits, so the launcher may run it with `sh`; the shebang line is then irrelevant).
7. That 48 MiB of sample memory is safe next to the launcher on 1 GB RAM.
8. That `.userdata/tg5040/` is writable and persistent across NextUI updates, and that writes to the log are cheap enough on the SD card.
9. Audio latency, underruns and the input profile (ADR 0016); USB MIDI is out of scope here.
