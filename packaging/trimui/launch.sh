#!/bin/sh

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# PicoTracker launcher for Trimui Smart Pro (tg5040). POSIX sh only.
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

# Display path (C-37, C-49). The binary links the device's vendor SDL2 dynamically and the
# app presents with SDL's GLES renderer, falling back to software itself. No SDL_VIDEODRIVER
# or SDL_RENDER_DRIVER is set: SDL picks the vendor driver, and the app prints a
# 'video driver: NAME' line into the log that shows which one it got.
# Device-verified (read from NextUI's MinUI.pak/launch.sh on the device, NextUI 20261013-0):
#   - NextUI exports LD_LIBRARY_PATH=$SYSTEM_PATH/lib:/usr/trimui/lib and sets no
#     SDL_VIDEODRIVER, SDL_RENDER_DRIVER or SDL_AUDIODRIVER for paks.
#   - /usr/trimui/lib holds libSDL2-2.0.so.0 2.30.8 (vendor 'mali' video driver), libEGL, libGLESv2.
#   - The old static SDL 2.26.1 had no usable driver there (offscreen fallback, black screen).
# NOT device-verified: a run of this packaged binary against /usr/trimui/lib (the device was
# unresponsive after the C-37 fb0 hang), and whether NextUI's own screen needs to be paused
# or handed off before a pak draws; the C-37 probe did not call for it. Nothing from
# /usr/trimui/lib is bundled in the pak.
export SDL_NOMOUSE=1

# Libraries: a bundled lib/ first if one is present (none is shipped), then the vendor
# directory, which NextUI already puts on the path; set here too so a manual run works.
if [ -d "$PAK_DIR/lib" ]; then
    export LD_LIBRARY_PATH="$PAK_DIR/lib:${LD_LIBRARY_PATH:-}"
fi
case ":${LD_LIBRARY_PATH:-}:" in
    *:/usr/trimui/lib:*) ;;
    *) export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}/usr/trimui/lib" ;;
esac

export HOME="${HOME:-$USERDATA}"
cd "$PAK_DIR" || exit 1

# ALSA capture mixer: the app does not change these controls today, but a
# capture setup (gain, mic or line input) must never outlive the session, so
# the values are saved here and put back after the app exits (exit chord or a
# crash). Skipped when amixer is missing. Control names are the tg5040 codec's.
MIXER_SAVE="$DATA_DIR/mixer.saved"
MIXER_CONTROLS="ADC volume|MIC1 gain volume|MIC2 gain volume|ADC Swap"
mixer_save() {
    command -v amixer >/dev/null 2>&1 || return 0
    : > "$MIXER_SAVE"
    echo "$MIXER_CONTROLS" | tr '|' '\n' | while IFS= read -r name; do
        value="$(amixer -c 0 cget "name=$name" 2>/dev/null | sed -n 's/^ *: values=//p')"
        [ -n "$value" ] && printf '%s|%s\n' "$name" "$value" >> "$MIXER_SAVE"
    done
}
mixer_restore() {
    [ -s "$MIXER_SAVE" ] || return 0
    while IFS='|' read -r name value; do
        amixer -c 0 cset "name=$name" "$value" >/dev/null 2>&1
    done < "$MIXER_SAVE"
    rm -f "$MIXER_SAVE"
}
# A save file left by a killed launcher holds the real original values.
if [ -s "$MIXER_SAVE" ]; then mixer_restore; fi
mixer_save
trap 'mixer_restore; exit 143' TERM HUP INT

# --log-input (ADR 0017 section 3) is not passed: the binary rejects unknown
# options and exits 2, and the flag is not implemented yet.
# Sample memory is 384 MiB, measured on a device with NextUI running: the app's RSS was ~404 MB and
# MemAvailable stayed at ~398 MB, no swap, no OOM (planning/findings/trimui-sample-memory.md).
# A sample that does not fit in the remaining budget fails to load (ADR 0010).
# One built-in mic on MIC1 (left): Line In's right channel is noise, so copy L to R (C-36).
PICOTRACKER_LINEIN_MONO=1 "$PAK_DIR/bin/picotracker" \
    --display trimui \
    --data-dir "$DATA_DIR" \
    --sample-memory-mib 384 \
    >"$LOG_DIR/picotracker.log" 2>&1
status=$?
mixer_restore
exit $status
