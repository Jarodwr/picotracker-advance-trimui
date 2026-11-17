#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Emulated Trimui smoke: runs what ships (the pak zip from scripts/package-trimui.sh) on a real
# glibc 2.23 userland (Ubuntu 16.04 arm64 container) with the SDL dummy video and audio drivers.
# It checks, in order: the bare `--display trimui --smoke` boot; the pak's launch.sh with
# USERDATA_PATH set (first-run seeding, log rotation, user files kept, clean exit on the
# Select+Start+L1+R1 chord) and without it (the $SD_ROOT/.userdata/tg5040 fallback); and a direct
# `--log-input` run proving the scripted directions and the chord arrive as joystick input.
# Last, a recording step: Edit+Play opens the Record screen, Play records and stops, and the WAV must
# appear under the data root and load (a tone from an ALSA file plugin stands in for the mic;
# tests/trimui/record_check.sh records the real mic on the device).
#
# A display-guard step runs the packaged launch.sh with no SDL_VIDEODRIVER and no --allow-headless: it
# must exit non-zero (the app refuses a driver that draws nothing) unless a real driver exists, and the
# log's 'video driver:' line must name the driver. The same container checks tests/trimui/smoke.sh
# against fabricated device results: an offscreen/dummy/missing driver line fails it, --allow-headless
# accepts dummy, and a real driver name passes. The screenshot and hash checks run under dummy on request.
#
# Input comes from a virtual gamepad made with /dev/uinput (tests/trimui/uinput_inject.py), so the
# container runs --privileged. The binary has no injection hook of its own.
#
# usage: emulated_smoke.sh [--binary PATH]   (default: build/trimui/picotracker_desktop)
# Needs: an arm64 Docker host (or qemu binfmt), network for apt inside the container, zip on the host.
# Not covered (needs the device): the real js0/event mapping, KMS/DRM output, ALSA audio, NextUI
# launching the pak and its USERDATA_PATH, the VFAT SD card.
# shellcheck disable=SC2015 # A && ok || fail is intentional
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
ok() { echo "ok: $*"; }
say() { echo "[emulated-smoke] $*"; }

if [[ "${PICOTRACKER_EMULATED_INSIDE:-}" != 1 ]]; then
  binary="${project_root}/build/trimui/picotracker_desktop"
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --binary) [[ $# -ge 2 ]] || { echo "--binary needs a path" >&2; exit 1; }; binary="$2"; shift ;;
      -h | --help) sed -n '2,20p' "${BASH_SOURCE[0]}"; exit 0 ;;
      *) echo "unknown argument: $1" >&2; exit 1 ;;
    esac
    shift
  done
  [[ -f "${binary}" ]] || { echo "binary not found: ${binary} (run scripts/build-trimui.sh)" >&2; exit 1; }
  upstream="${PICOTRACKER_UPSTREAM_ROOT:-${project_root}/upstream/advance-firmware}"
  # A worktree has an empty submodule directory; fall back to the main checkout.
  if [[ ! -d "${upstream}/LICENSES" ]]; then
    common="$(git -C "${project_root}" rev-parse --path-format=absolute --git-common-dir)"
    upstream="$(dirname "${common}")/upstream/advance-firmware"
  fi
  work="$(mktemp -d)"
  trap 'rm -rf "${work}"' EXIT
  "${project_root}/scripts/package-trimui.sh" --binary "${binary}" --version smoke --upstream "${upstream}" \
    --out "${work}/pak.zip" >"${work}/package.log" 2>&1 || { cat "${work}/package.log" >&2; exit 1; }
  cp "${binary}" "${work}/picotracker_desktop"
  # The binary links SDL2 dynamically; the link stub built next to it stands in for the vendor library.
  stub="$(dirname "${binary}")/sdl-stub"
  [[ -d "${stub}" ]] || { echo "SDL link stub not found: ${stub} (run scripts/build-trimui.sh)" >&2; exit 1; }
  cp -a "${stub}" "${work}/sdl-stub"
  docker run --rm --privileged --platform linux/arm64 \
    -v "${project_root}:/src:ro" -v "${work}:/work" \
    -e PICOTRACKER_EMULATED_INSIDE=1 ubuntu:16.04 bash /src/tests/trimui/emulated_smoke.sh
  exit $?
fi

# ---- inside the container ----
export DEBIAN_FRONTEND=noninteractive
apt-get update -qq >/dev/null && apt-get install -y -qq --no-install-recommends unzip python3 libasound2 >/dev/null
# The dummy video driver never gives its window keyboard focus, and SDL drops joystick events
# while a window exists without focus unless this hint is set.
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1
export LD_LIBRARY_PATH=/work/sdl-stub
ldd --version | sed -n 1p

sd=/tmp/sd
mkdir -p "${sd}"
unzip -q /work/pak.zip -d "${sd}"
pak="${sd}/Tools/tg5040/PicoTracker.pak"
[[ -x "${pak}/launch.sh" && -x "${pak}/bin/picotracker" ]] && ok "unzipped pak" || { fail "pak layout"; exit 1; }

# 1. Bare boot on the fresh binary, trimui display mode.
if /work/picotracker_desktop --display trimui --smoke --data-dir /tmp/smoke-data >/tmp/boot.log 2>&1; then
  ok "--display trimui --smoke exits 0"
else
  cat /tmp/boot.log; fail "--display trimui --smoke"
fi

# 1b. Screenshot and hash checks under the dummy driver, requested explicitly (SDL_VIDEODRIVER, and
# --allow-headless with the variable unset): --smoke compares frame hashes, --screenshot saves the frame.
rm -f /tmp/shot-env.bmp /tmp/shot-flag.bmp
if /work/picotracker_desktop --display trimui --smoke --screenshot /tmp/shot-env.bmp --data-dir /tmp/smoke-data >/tmp/shot-env.log 2>&1 \
  && [[ -s /tmp/shot-env.bmp ]] && grep -q "^firmware frame hash: " /tmp/shot-env.log; then
  ok "SDL_VIDEODRIVER=dummy: hash check and screenshot pass ($(wc -c </tmp/shot-env.bmp | tr -d ' ') bytes)"
else
  cat /tmp/shot-env.log; fail "dummy driver (SDL_VIDEODRIVER): hash/screenshot check"
fi
if env -u SDL_VIDEODRIVER /work/picotracker_desktop --display trimui --allow-headless --smoke --screenshot /tmp/shot-flag.bmp \
  --data-dir /tmp/smoke-data >/tmp/shot-flag.log 2>&1 && [[ -s /tmp/shot-flag.bmp ]] && grep -q "^firmware frame hash: " /tmp/shot-flag.log; then
  ok "--allow-headless: hash check and screenshot pass"
else
  cat /tmp/shot-flag.log; fail "--allow-headless: hash/screenshot check"
fi
grep -q "^video driver: dummy$" /tmp/shot-env.log && ok "video driver line present: dummy (explicit)" || fail "no 'video driver: dummy' line with SDL_VIDEODRIVER=dummy"

# 1c. Display guard: the packaged launch.sh with no SDL_VIDEODRIVER and no --allow-headless. launch.sh
# sets neither, so with only a headless driver available the app must refuse (non-zero, never hang).
guard_ud=/tmp/userdata-guard
guard_status=0
env -u SDL_VIDEODRIVER USERDATA_PATH="${guard_ud}" timeout 30 sh "${pak}/launch.sh" || guard_status=$?
guard_log="${guard_ud}/PicoTracker/logs/picotracker.log"
guard_driver="$(sed -n 's/^video driver: //p' "${guard_log}" 2>/dev/null | head -n 1)"
# A real driver (anything but offscreen, dummy, none or an empty name) would let the app start; with the
# stub SDL there is none, so a non-zero status is required. If one ever exists, require only the log line.
headless_driver() { case "$1" in offscreen | dummy | none | "") return 0 ;; *) return 1 ;; esac; }
if [[ "${guard_status}" == 124 ]]; then
  fail "launch.sh without SDL_VIDEODRIVER did not exit (timed out): the display guard is missing"
elif headless_driver "${guard_driver}"; then
  [[ "${guard_status}" != 0 ]] && ok "launch.sh without SDL_VIDEODRIVER exits ${guard_status} on the '${guard_driver:-no}' driver" \
    || fail "launch.sh without SDL_VIDEODRIVER exited 0 on a headless driver ('${guard_driver:-no}')"
  [[ "${guard_status}" == 3 ]] && ok "guard exit status is 3" || echo "note: status ${guard_status} (3 is the guard; 1 is SDL finding no driver at all)"
else
  ok "a real video driver exists here ('${guard_driver}'): the guard need not fire"
fi
[[ -n "${guard_driver}" || "${guard_status}" == 1 ]] && ok "guard run log: driver '${guard_driver:-none}' (status ${guard_status})" \
  || fail "launch.sh log has no 'video driver:' line (status ${guard_status})"
rm -rf "${guard_ud}"

# 1d. tests/trimui/smoke.sh against fabricated device results (--sd-root): the driver line decides.
fake_sd() { # name RESULT driver-line-or-empty
  local d="/tmp/fake-sd-$1/.userdata/tg5040/PicoTracker/smoke"
  mkdir -p "${d}"; echo 0 >"${d}/RESULT"; echo bmp >"${d}/smoke.bmp"
  { echo "boot"; [[ -n "$2" ]] && echo "video driver: $2"; echo "firmware frame hash: 1 -> 2"; } >"${d}/smoke.log"
  echo "/tmp/fake-sd-$1"
}
for drv in offscreen dummy; do
  if bash /src/tests/trimui/smoke.sh --sd-root "$(fake_sd "${drv}" "${drv}")" --out-dir "/tmp/fake-out-${drv}" >/tmp/fake-${drv}.log 2>&1; then
    cat /tmp/fake-${drv}.log; fail "smoke.sh passed with video driver ${drv}"
  else ok "smoke.sh fails on video driver ${drv}"; fi
done
if bash /src/tests/trimui/smoke.sh --sd-root "$(fake_sd none "")" --out-dir /tmp/fake-out-none >/tmp/fake-none.log 2>&1; then
  fail "smoke.sh passed with no video driver line"; else ok "smoke.sh fails with no video driver line"; fi
bash /src/tests/trimui/smoke.sh --allow-headless --sd-root "$(fake_sd dummy "dummy")" --out-dir /tmp/fake-out-ah >/tmp/fake-ah.log 2>&1 \
  && ok "smoke.sh --allow-headless accepts dummy" || { cat /tmp/fake-ah.log; fail "smoke.sh --allow-headless rejected dummy"; }
bash /src/tests/trimui/smoke.sh --sd-root "$(fake_sd mali mali)" --out-dir /tmp/fake-out-mali >/tmp/fake-mali.log 2>&1 \
  && ok "smoke.sh passes on a real driver (mali)" || { cat /tmp/fake-mali.log; fail "smoke.sh rejected the mali driver"; }

# 2. Virtual gamepad.
fifo=/tmp/pad.fifo
mkfifo "${fifo}"
python3 -I /src/tests/trimui/uinput_inject.py "${fifo}" >/tmp/inject.log 2>&1 &
inject_pid=$!
for _ in $(seq 50); do grep -q '^node ' /tmp/inject.log 2>/dev/null && break; sleep 0.1; done
grep -q '^node ' /tmp/inject.log || { cat /tmp/inject.log; fail "virtual gamepad not created"; exit 1; }
ok "virtual gamepad $(cut -d' ' -f2 /tmp/inject.log)"
pad() { printf '%s\n' "$@" >"${fifo}"; }

# Starts "$@" in the background, plays a few directions, then repeats the chord until it exits
# (events sent before SDL has opened the pad are lost, hence the repeat). Sets $status.
run_scripted() {
  "$@" &
  local pid=$! tries=0
  sleep 4
  pad "hat down" "hat down" "hat right" "hat up" "hat left"
  while kill -0 "${pid}" 2>/dev/null && [[ ${tries} -lt 25 ]]; do
    pad "chord"
    tries=$((tries + 1))
    sleep 2 # the injector needs about 1.2 s per chord; do not let commands queue up
  done
  if kill -0 "${pid}" 2>/dev/null; then kill "${pid}" 2>/dev/null || true; status=timeout; wait "${pid}" 2>/dev/null || true
  else wait "${pid}" && status=0 || status=$?; fi
}

ud=/tmp/userdata
data="${ud}/PicoTracker"

# 3. launch.sh with USERDATA_PATH: first run seeds, exits on the chord.
export USERDATA_PATH="${ud}"
run_scripted sh "${pak}/launch.sh"
[[ "${status}" == 0 ]] && ok "run 1 (USERDATA_PATH) exited 0 on the chord" || { fail "run 1 status ${status}"; tail -n 20 "${data}/logs/picotracker.log" || true; }
for f in .seeded projects/.keep samples/.keep logs/picotracker.log; do
  [[ -e "${data}/${f}" ]] && ok "run 1 created ${f}" || fail "run 1 missing ${f}"
done
grep -q "^video driver: dummy$" "${data}/logs/picotracker.log" && ok "run 1 log has 'video driver: dummy'" || fail "run 1 log has no video driver line"
cp "${data}/logs/picotracker.log" /tmp/log-run1
[[ ! -e "${data}/logs/picotracker.log.1" ]] && ok "no picotracker.log.1 after the first run" || fail "unexpected picotracker.log.1 on run 1"

# 4. Second run: user files survive (even if seeding is forced again) and the log rotates.
echo "mine" >"${data}/projects/mine.sav"
echo "edited" >"${data}/projects/.keep"
rm "${data}/.seeded"
run_scripted sh "${pak}/launch.sh"
[[ "${status}" == 0 ]] && ok "run 2 exited 0 on the chord" || fail "run 2 status ${status}"
[[ "$(cat "${data}/projects/mine.sav")" == mine ]] && ok "user project kept" || fail "user project lost"
[[ "$(cat "${data}/projects/.keep")" == edited ]] && ok "seeding did not overwrite a user file" || fail "seeded file overwrote user edit"
[[ -e "${data}/.seeded" ]] && ok ".seeded recreated" || fail ".seeded missing after run 2"
if [[ -f "${data}/logs/picotracker.log.1" ]] && cmp -s "${data}/logs/picotracker.log.1" /tmp/log-run1; then
  ok "picotracker.log.1 is run 1's log"
else
  fail "picotracker.log.1 is not run 1's log"
fi
[[ -s "${data}/logs/picotracker.log" ]] && ok "picotracker.log written by run 2" || fail "run 2 log is empty"

# 5. Without USERDATA_PATH: the $SD_ROOT/.userdata/tg5040 fallback.
unset USERDATA_PATH
run_scripted sh "${pak}/launch.sh"
[[ "${status}" == 0 ]] && ok "run 3 (no USERDATA_PATH) exited 0 on the chord" || fail "run 3 status ${status}"
fallback="${sd}/.userdata/tg5040/PicoTracker"
for f in .seeded projects/.keep logs/picotracker.log; do
  [[ -e "${fallback}/${f}" ]] && ok "fallback has ${f}" || fail "fallback missing ${f}"
done

# 6. Direct run with --log-input: the directions and the chord reach the joystick source.
run_scripted /work/picotracker_desktop --display trimui --log-input --data-dir /tmp/direct-data \
  --sample-memory-mib 48 2>/tmp/direct.log
[[ "${status}" == 0 ]] && ok "run 4 (--log-input) exited 0 on the chord" || fail "run 4 status ${status}"
grep -q "input: joystick" /tmp/direct.log && ok "pad detected: $(grep -m1 'input: joystick' /tmp/direct.log)" || fail "pad not detected"
for d in 1 2 3 4; do # hat directions 1..4 = up, down, left, right
  grep -q "^input: hat 0 ${d}$" /tmp/direct.log && ok "direction ${d} logged" || fail "direction ${d} not logged"
done
for b in 6 7 4 5; do
  grep -q "^input: button ${b}$" /tmp/direct.log && ok "chord button ${b} logged" || fail "chord button ${b} not logged"
done

# 7. Recording through the pak: Edit+Play (B+Start) opens the Record screen, Play starts and stops a
# recording. The SDL dummy driver delivers silence and the firmware's record threshold never fires on
# silence, so this step uses SDL's ALSA backend (the one the device uses) with an ~/.asoundrc whose
# default capture is the ALSA "file" plugin reading a 440 Hz tone from a raw file, and whose playback
# goes to the null device. The file is a FIFO fed with the tone in real time, because the plugin is not
# paced and a plain file would be at its end before Play is pressed. The WAV must appear
# under the data root's recordings/, load, and carry the tone (non-zero RMS); its length is not
# checked tightly (the FIFO is paced by sleep, not a clock).
mkfifo /tmp/tone.fifo
# A real-time paced producer: 0.1 s of a 440 Hz tone (stereo S16, 44100 Hz) every 0.1 s, reopening
# the FIFO whenever the reader closes it (the app opens and closes the capture more than once).
python3 -I -c "
import math, struct, time
chunk = b''.join(struct.pack('<hh', *(int(8000 * math.sin(i * 2 * math.pi * 440 / 44100)),) * 2) for i in range(4410))
while True:
    try:
        out = open('/tmp/tone.fifo', 'wb', 0)
        while True:
            out.write(chunk)
            time.sleep(0.1)
    except (BrokenPipeError, OSError):
        pass" 2>/dev/null &
tone_pid=$!
cat >/root/.asoundrc <<'RC'
pcm.!default { type asym; playback.pcm "null"; capture.pcm "tone" }
pcm.tone { type file; slave.pcm "null"; file "/dev/null"; infile "/tmp/tone.fifo"; format "raw" }
RC
export SDL_AUDIODRIVER=alsa HOME=/root
ud_rec=/tmp/userdata-rec
export USERDATA_PATH="${ud_rec}"
sh "${pak}/launch.sh" &
rec_pid=$!
sleep 6
pad "button 0 7" # Edit+Play: Record screen
sleep 3
pad "button 7" # start
sleep 4
pad "button 7" # stop and save
sleep 4
rec_wav="${ud_rec}/PicoTracker/recordings/REC01.wav"
[[ -s "${rec_wav}" ]] && ok "recording wrote recordings/REC01.wav under the data root" || fail "no REC01.wav under the data root"
python3 -I /src/tests/trimui/wav_check.py "${rec_wav}" 4 --any-length >/tmp/rec-wav.txt && ok "REC01.wav loads: $(tr '\n' ' ' </tmp/rec-wav.txt | cut -c1-120)" \
  || { cat /tmp/rec-wav.txt; fail "REC01.wav check"; }
grep -q "SAMPLEEDITOR\] Loaded .* frames from /recordings/REC01.wav" "${ud_rec}/PicoTracker/logs/picotracker.log" \
  && ok "the app loaded the new recording into the sample editor" || fail "app log shows no sample editor load of REC01.wav"
if [[ ! -s "${rec_wav}" ]]; then echo "---- recording run log ----"; ls -la "${ud_rec}/PicoTracker/recordings" | head -n 10; grep -a "recording:\|audio:\|RECORD\|input:\|ERROR" "${ud_rec}/PicoTracker/logs/picotracker.log" | head -n 20; fi
tries=0
while kill -0 "${rec_pid}" 2>/dev/null && [[ ${tries} -lt 25 ]]; do pad chord; tries=$((tries + 1)); sleep 2; done
if kill -0 "${rec_pid}" 2>/dev/null; then kill "${rec_pid}" 2>/dev/null || true; fail "recording run did not exit on the chord"
else wait "${rec_pid}" && ok "recording run exited 0 on the chord" || fail "recording run exit status $?"; fi
kill "${tone_pid}" 2>/dev/null || true

pad quit || true
wait "${inject_pid}" 2>/dev/null || true
if [[ ${failures} -gt 0 ]]; then echo "emulated smoke: ${failures} failure(s)" >&2; echo "---- direct.log ----"; head -n 30 /tmp/direct.log; exit 1; fi
say "emulated smoke passed on glibc 2.23"
