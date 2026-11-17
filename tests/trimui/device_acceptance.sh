#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# shellcheck disable=SC2029,SC2015,SC2016 # remote commands are built on this side; A && ok || fail is intentional

# On-device acceptance for the Trimui Smart Pro with the real display and the deployed pak
# (planning/findings/trimui-acceptance.md). Wraps record_check.sh, memory_probe.sh and display_check.sh
# (not edited) and adds a long run. Prints PASS or FAIL per item:
#   wedge      /dev/fb0 is not wedged (no process stuck in fb_open/lock_fb_info); otherwise nothing
#              else runs and the script exits 2 (BLOCKED): only a reboot clears a wedged fb0.
#   display    display_check.sh passes (picture on the panel, chord exit, NextUI processes keep running)
#   long-run   the app runs --minutes (default 5) under launch.sh with a real 'video driver:' line
#   audio      no underrun/overrun/xrun line in the app log of the long run after start-up (before the
#              'video driver:' line is reported only)
#   memory     RSS and MemAvailable sampled during the long run; no new OOM line in dmesg; memory_probe.sh
#              at the 384 MiB budget: all probes loaded or refused as designed, no OOM, no swap, alive
#   record     record_check.sh --skip-deploy passes
#   mixer      ALSA mixer identical before and after the long run
#   chord-exit the chord ends launch.sh with status 0 and NextUI's processes are still running
#   relaunch   a second launch runs, then exits 0 on the chord
# Deploys nothing: it tests the pak already on the card ("latest deployed pak").
#
# usage: device_acceptance.sh [--ssh USER@HOST[:SD_ROOT]] [--minutes N] [--out-dir DIR]
#   --ssh defaults to $TRIMUI_SSH_USER@$TRIMUI_SSH_HOST (user root). Auth: TRIMUI_SSH_PASSWORD through a
#   throwaway SSH_ASKPASS helper (never written elsewhere), TRIMUI_SSH_PORT if not 22.
# Needs python3 and docker on this machine (the gamepad injector, first run only, via record_check.sh).
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ssh_target=""
minutes=5
out_dir=""
remote_tmp=/tmp/picotracker-acceptance

die() { echo "device_acceptance: $*" >&2; exit 1; }
failures=0
pass() { echo "PASS $1: $2"; }
fail() { echo "FAIL $1: $2" >&2; failures=$((failures + 1)); }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ssh) [[ $# -ge 2 ]] || die "--ssh needs USER@HOST"; ssh_target="$2"; shift ;;
    --minutes) [[ $# -ge 2 ]] || die "--minutes needs a number"; minutes="$2"; shift ;;
    --out-dir) [[ $# -ge 2 ]] || die "--out-dir needs a directory"; out_dir="$2"; shift ;;
    -h | --help) sed -n '3,27p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done
[[ -n "${ssh_target}" ]] || ssh_target="${TRIMUI_SSH_USER:-root}@${TRIMUI_SSH_HOST:?set TRIMUI_SSH_HOST or give --ssh}"
[[ "${minutes}" =~ ^[0-9]+$ && ${minutes} -ge 1 ]] || die "--minutes needs a whole number >= 1"
ssh_host="${ssh_target%%:*}"
[[ "${ssh_target}" == *:* && "${ssh_target#*:}" != /mnt/SDCARD ]] && die "the device script assumes the SD card at /mnt/SDCARD"
[[ -n "${out_dir}" ]] || out_dir="${project_root}/build/trimui-acceptance/$(date +%Y%m%d-%H%M%S)"
mkdir -p "${out_dir}"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8)
scp_opts=("${ssh_opts[@]}")
if [[ -n "${TRIMUI_SSH_PASSWORD:-}" ]]; then
  printf '#!/bin/sh\necho "$TRIMUI_SSH_PASSWORD"\n' >"${work}/askpass"
  chmod +x "${work}/askpass"
  export SSH_ASKPASS="${work}/askpass" SSH_ASKPASS_REQUIRE=force DISPLAY="${DISPLAY:-none}"
  ssh_opts+=(-o PubkeyAuthentication=no); scp_opts+=(-o PubkeyAuthentication=no)
fi
if [[ -n "${TRIMUI_SSH_PORT:-}" ]]; then ssh_opts+=(-p "${TRIMUI_SSH_PORT}"); scp_opts+=(-P "${TRIMUI_SSH_PORT}"); fi
rssh() { ssh "${ssh_opts[@]}" "${ssh_host}" "$@"; }
rscp() { scp "${scp_opts[@]}" "$@"; }

# 0. A wedged fb0 hangs every opener for good (D state, unkillable): stop before starting anything.
wedged="$(rssh 'for p in /proc/[0-9]*; do w=$(cat $p/wchan 2>/dev/null); case "$w" in fb_open|lock_fb_info|fb_*) echo "${p#/proc/}:$w:$(cat $p/comm 2>/dev/null)";; esac; done' 2>/dev/null || true)"
if [[ -n "${wedged}" ]]; then
  fail wedge "/dev/fb0 is wedged (pid:wchan:name) $(echo "${wedged}" | tr '\n' ' '): only a reboot clears it; nothing was run"
  echo "BLOCKED: device acceptance needs a working fb0" >&2
  exit 2
fi
pass wedge "no process stuck in the framebuffer driver"

# 1. Injector, then the long run and the relaunch on the device (busybox sh, on stdin).
injector="${project_root}/build/trimui-record/uinput_inject"
if [[ ! -x "${injector}" || "${project_root}/tests/trimui/uinput_inject.c" -nt "${injector}" ]]; then
  mkdir -p "$(dirname "${injector}")"
  docker build -q -t picotracker-trimui-build:1 "${project_root}/docker/trimui" >/dev/null
  docker run --rm -v "${project_root}:/src:ro" -v "$(dirname "${injector}"):/out" picotracker-trimui-build:1 \
    trimui-cc -O2 -Wall -o /out/uinput_inject /src/tests/trimui/uinput_inject.c
fi
rssh "rm -rf ${remote_tmp} && mkdir -p ${remote_tmp}"
rscp "${injector}" "${ssh_host}:${remote_tmp}/"

device_script() {
  cat <<'EOF'
SECS="$1"; OUT="$2"; TOOLS="$3"
SD=/mnt/SDCARD
PAK="$SD/Tools/tg5040/PicoTracker.pak"
DATA="$SD/.userdata/tg5040/PicoTracker"
mkdir -p "$OUT"
kb() { awk -v k="$1" '$1 == k":" { print $2 }' "${2:-/proc/meminfo}"; }
mixer() { amixer -c 0 contents 2>/dev/null; }
nextui() { ps w | grep -E 'MinUI|nextui|trimui_inputd|keymon|audiomon' | grep -v grep | sed 's/^ *[0-9]* //' | sort; }
FIFO="$TOOLS/pad.fifo"; rm -f "$FIFO"; mkfifo "$FIFO"
chmod +x "$TOOLS/uinput_inject"
"$TOOLS/uinput_inject" "$FIFO" >"$OUT/inject.log" 2>&1 &
inject_pid=$!
i=0; while ! grep -q ready "$OUT/inject.log" 2>/dev/null && [ "$i" -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
grep -q ready "$OUT/inject.log" || { echo "virtual gamepad not created" >"$OUT/error"; exit 0; }
pad() { printf '%s\n' "$@" >"$FIFO"; }
export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1

# run NAME SECONDS: launch.sh, sample memory every 15 s, leave on the chord, record the exit status.
run() {
  name="$1"; secs="$2"
  oom0=$(dmesg | grep -c -i -E 'out of memory|oom-kill|Killed process')
  mixer >"$OUT/$name.mixer.before"; nextui >"$OUT/$name.nextui.before"
  sh "$PAK/launch.sh" &
  app=$!
  t=0; : >"$OUT/$name.samples"
  while [ "$t" -lt "$secs" ]; do
    sleep 5; t=$((t + 5))
    if [ $((t % 15)) -eq 0 ]; then
      pid=$(pidof picotracker | cut -d' ' -f1)
      echo "$t	$(kb VmRSS /proc/${pid:-0}/status)	$(kb VmHWM /proc/${pid:-0}/status)	$(kb MemAvailable)	$(( $(kb SwapTotal) - $(kb SwapFree) ))" >>"$OUT/$name.samples"
    fi
    kill -0 "$app" 2>/dev/null || { echo "app exited early at ${t}s" >>"$OUT/$name.notes"; break; }
  done
  cp "$DATA/logs/picotracker.log" "$OUT/$name.log" 2>/dev/null
  tries=0
  while kill -0 "$app" 2>/dev/null && [ "$tries" -lt 15 ]; do pad chord; tries=$((tries + 1)); sleep 2; done
  if kill -0 "$app" 2>/dev/null; then echo timeout >"$OUT/$name.exit"; killall picotracker 2>/dev/null; wait "$app" 2>/dev/null
  else wait "$app"; echo "$?" >"$OUT/$name.exit"; fi
  cp "$DATA/logs/picotracker.log" "$OUT/$name.log.final" 2>/dev/null
  mixer >"$OUT/$name.mixer.after"; nextui >"$OUT/$name.nextui.after"
  echo $(( $(dmesg | grep -c -i -E 'out of memory|oom-kill|Killed process') - oom0 )) >"$OUT/$name.oom"
}
run long "$SECS"
sleep 3
run relaunch 12
pad quit; sleep 1; kill "$inject_pid" 2>/dev/null; rm -f "$FIFO"
EOF
}
echo "[acceptance] long run: ${minutes} min, then a relaunch"
device_script | rssh "sh -s -- $((minutes * 60)) ${remote_tmp}/out ${remote_tmp}" >"${out_dir}/device.log" 2>&1 \
  || fail long-run "device run failed (see ${out_dir}/device.log)"
rscp -r "${ssh_host}:${remote_tmp}/out/." "${out_dir}/" >/dev/null 2>&1 || fail long-run "could not copy the results back"
rssh "rm -rf ${remote_tmp}" || true

# 2. Judge the long run.
log="${out_dir}/long.log.final"
[[ ! -f "${out_dir}/error" ]] || fail long-run "$(cat "${out_dir}/error")"
if [[ -f "${out_dir}/long.log" ]] && grep -q 'video driver:' "${out_dir}/long.log" && ! grep -q 'video driver: \(offscreen\|dummy\)' "${out_dir}/long.log" \
  && [[ ! -f "${out_dir}/long.notes" ]]; then
  pass long-run "ran ${minutes} min; $(grep -m1 'video driver:' "${out_dir}/long.log")"
else
  fail long-run "no real video driver line, or the app exited early ($(cat "${out_dir}/long.notes" 2>/dev/null || echo "no notes"))"
fi
if [[ -f "${log}" ]]; then
  # Start-up lines (before the 'video driver:' line, i.e. before the window exists and playback runs) are
  # reported but not counted: C-58 saw a single ALSA underrun there from the device open, none afterwards.
  n="$(awk 'seen && tolower($0) ~ /underrun|overrun|xrun/ { c++ } /video driver:/ { seen = 1 } END { print c + 0 }' "${log}")"
  early="$(awk '!seen && tolower($0) ~ /underrun|overrun|xrun/ { c++ } /video driver:/ { seen = 1 } END { print c + 0 }' "${log}")"
  [[ "${early}" == 0 ]] || echo "[acceptance] note: ${early} start-up underrun line(s) before the window was created (not counted)"
  [[ "${n}" == 0 ]] && pass audio "no underrun/overrun/xrun line after start-up in the long-run log" || fail audio "${n} underrun/overrun/xrun line(s) after start-up"
else fail audio "no app log"; fi
if [[ -s "${out_dir}/long.samples" ]]; then
  peak="$(awk -F'\t' '{ if ($3 > m) m = $3 } END { print int(m / 1024) }' "${out_dir}/long.samples")"
  low="$(awk -F'\t' 'NR == 1 || $4 < m { m = $4 } END { print int(m / 1024) }' "${out_dir}/long.samples")"
  swap="$(awk -F'\t' '{ if ($5 > m) m = $5 } END { print m + 0 }' "${out_dir}/long.samples")"
  echo "long run: peak RSS ${peak} MB, lowest MemAvailable ${low} MB, max swap ${swap} KB"
  [[ "$(cat "${out_dir}/long.oom" 2>/dev/null || echo 1)" == 0 && "${swap}" == 0 ]] \
    && pass memory "long run: no OOM line in dmesg, no swap" || fail memory "OOM line(s) in dmesg or swap in use during the long run"
else fail memory "no memory samples"; fi
cmp -s "${out_dir}/long.mixer.before" "${out_dir}/long.mixer.after" && [[ -s "${out_dir}/long.mixer.before" ]] \
  && pass mixer "ALSA mixer identical before and after" || fail mixer "mixer differs or no snapshot (amixer missing?)"
if [[ "$(cat "${out_dir}/long.exit" 2>/dev/null)" == 0 ]] && cmp -s "${out_dir}/long.nextui.before" "${out_dir}/long.nextui.after" \
  && [[ -s "${out_dir}/long.nextui.after" ]]; then
  pass chord-exit "launch.sh exited 0 on the chord; NextUI processes unchanged"
else fail chord-exit "exit status $(cat "${out_dir}/long.exit" 2>/dev/null || echo missing); NextUI processes before/after differ or absent"; fi
if [[ "$(cat "${out_dir}/relaunch.exit" 2>/dev/null)" == 0 ]] && grep -q 'video driver:' "${out_dir}/relaunch.log" 2>/dev/null; then
  pass relaunch "second launch ran and exited 0"
else fail relaunch "second launch: exit $(cat "${out_dir}/relaunch.exit" 2>/dev/null || echo missing)"; fi

# 3. The existing checks, unedited.
t="${project_root}/tests/trimui"
"${t}/display_check.sh" --ssh "${ssh_target}" --skip-deploy --out-dir "${out_dir}/display" >"${out_dir}/display.txt" 2>&1 \
  && pass display "display_check.sh" || fail display "display_check.sh (see ${out_dir}/display.txt)"
"${t}/record_check.sh" --ssh "${ssh_target}" --skip-deploy --out-dir "${out_dir}/record" >"${out_dir}/record.txt" 2>&1 \
  && pass record "record_check.sh" || fail record "record_check.sh (see ${out_dir}/record.txt)"
"${t}/memory_probe.sh" --budgets "384" >"${out_dir}/memory_probe.txt" 2>&1 \
  && awk -F'\t' 'NR == 2 { exit !($13 == 0 && $11 == 0 && $14 == "yes") }' "${out_dir}/memory_probe.txt" \
  && pass memory-384 "memory_probe.sh at 384 MiB: no OOM, no swap, alive" || fail memory-384 "memory_probe.sh (see ${out_dir}/memory_probe.txt)"

echo "results: ${out_dir}"
if [[ ${failures} -gt 0 ]]; then echo "device acceptance: ${failures} failure(s)" >&2; exit 1; fi
echo "device acceptance: all items PASS"
