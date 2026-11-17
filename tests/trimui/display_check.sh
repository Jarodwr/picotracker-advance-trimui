#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# shellcheck disable=SC2029,SC2015,SC2016 # remote commands are built on this side; A && ok || fail is intentional

# Display check for the Trimui Smart Pro: proves the app draws on the panel. Starts the installed
# pak's launch.sh the way NextUI does, waits, reads the visible page of /dev/fb0 and checks it on
# this machine (tests/trimui/display_check.py): not black, a 720x720 content region centred in the
# 1280x720 panel with black borders, and roughly the same picture as the --smoke screenshot of the
# same screen. Also checks the app log (a real 'video driver:' and the panel size), the CPU use of the
# picotracker process, and that the exit chord (virtual gamepad, uinput_inject) ends the process
# with status 0 while NextUI's own processes keep running. The app is not left running.
#
# usage: display_check.sh --ssh USER@HOST[:SD_ROOT] [--wait N] [--out-dir DIR] [--skip-deploy] [--binary PATH]
#   --wait N        seconds between starting the app and capturing (default 8).
#   --skip-deploy   reuse the pak on the card (default: scripts/deploy-trimui.sh first).
#
# fb0 safety: on some boots /dev/fb0 wedges (an open() or ioctl waits forever in the kernel, in
# D state, and cannot be killed). The device side therefore looks for a process already stuck in
# fb_open/lock_fb_info before touching fb0, and reads fb0 only from a background dd it never waits
# on for more than 10 s. A wedged fb0 is reported as a FAIL with the reason; only a reboot clears it.
#
# Auth: TRIMUI_SSH_PASSWORD is used through a throwaway SSH_ASKPASS helper in a temp dir (never
# written anywhere else); without it ssh needs a key. Needs python3 and docker (injector, first run).
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ssh_target=""
wait_s=8
out_dir=""
skip_deploy=0
binary="${project_root}/build/trimui/picotracker_desktop"
remote_tmp=/tmp/picotracker-display-check

die() { echo "display_check: $*" >&2; exit 1; }
say() { echo "[display-check] $*"; }
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
ok() { echo "ok: $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ssh) [[ $# -ge 2 ]] || die "--ssh needs USER@HOST"; ssh_target="$2"; shift ;;
    --wait) [[ $# -ge 2 ]] || die "--wait needs a number"; wait_s="$2"; shift ;;
    --out-dir) [[ $# -ge 2 ]] || die "--out-dir needs a directory"; out_dir="$2"; shift ;;
    --binary) [[ $# -ge 2 ]] || die "--binary needs a path"; binary="$2"; shift ;;
    --skip-deploy) skip_deploy=1 ;;
    -h | --help) sed -n '3,24p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done
[[ -n "${ssh_target}" ]] || die "give --ssh USER@HOST[:SD_ROOT]"
[[ "${wait_s}" =~ ^[0-9]+$ && ${wait_s} -ge 3 ]] || die "--wait needs a whole number >= 3"

ssh_host="${ssh_target%%:*}"
sd_root="/mnt/SDCARD"
[[ "${ssh_target}" == *:* ]] && sd_root="${ssh_target#*:}"
[[ "${sd_root}" == /mnt/SDCARD ]] || die "the device script assumes the SD card at /mnt/SDCARD"
[[ -n "${out_dir}" ]] || out_dir="${project_root}/build/trimui-display/$(date +%Y%m%d-%H%M%S)"
mkdir -p "${out_dir}"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
if [[ -n "${TRIMUI_SSH_PASSWORD:-}" ]]; then
  printf '#!/bin/sh\necho "$TRIMUI_SSH_PASSWORD"\n' >"${work}/askpass"
  chmod +x "${work}/askpass"
  export SSH_ASKPASS="${work}/askpass" SSH_ASKPASS_REQUIRE=force DISPLAY="${DISPLAY:-none}"
  auth_opts=(-o PubkeyAuthentication=no)
else
  auth_opts=()
fi
ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 ${auth_opts[@]+"${auth_opts[@]}"})
if [[ -n "${TRIMUI_SSH_PORT:-}" ]]; then ssh_opts+=(-p "${TRIMUI_SSH_PORT}"); fi
scp_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 ${auth_opts[@]+"${auth_opts[@]}"})
if [[ -n "${TRIMUI_SSH_PORT:-}" ]]; then scp_opts+=(-P "${TRIMUI_SSH_PORT}"); fi
rssh() { ssh "${ssh_opts[@]}" "${ssh_host}" "$@"; }
rscp() { scp "${scp_opts[@]}" "$@"; }

# 1. The gamepad injector for the device (aarch64, built in the toolchain image, cached).
injector="${project_root}/build/trimui-record/uinput_inject"
if [[ ! -x "${injector}" || "${project_root}/tests/trimui/uinput_inject.c" -nt "${injector}" ]]; then
  say "building the on-device gamepad injector"
  mkdir -p "$(dirname "${injector}")"
  docker build -q -t picotracker-trimui-build:1 "${project_root}/docker/trimui" >/dev/null
  docker run --rm -v "${project_root}:/src:ro" -v "$(dirname "${injector}"):/out" picotracker-trimui-build:1 \
    trimui-cc -O2 -Wall -o /out/uinput_inject /src/tests/trimui/uinput_inject.c
fi

# 2. Deploy (scripts/deploy-trimui.sh reads the same ssh environment), then the helpers in /tmp.
if [[ ${skip_deploy} -eq 0 ]]; then
  say "deploying the pak"
  "${project_root}/scripts/deploy-trimui.sh" --binary "${binary}" --ssh "${ssh_target}" >"${out_dir}/deploy.log" 2>&1 \
    || { cat "${out_dir}/deploy.log" >&2; die "deploy failed"; }
fi
rssh "rm -rf ${remote_tmp} && mkdir -p ${remote_tmp}"
rscp "${injector}" "${ssh_host}:${remote_tmp}/"

# 3. The device side (busybox sh), sent on stdin.
device_script() {
  cat <<'EOF'
WAIT="$1"; OUT="$2"; TOOLS="$3"
SD=/mnt/SDCARD
PAK="$SD/Tools/tg5040/PicoTracker.pak"
DATA="$SD/.userdata/tg5040/PicoTracker"
mkdir -p "$OUT"
say() { echo "$*" >>"$OUT/device.log"; }

# NextUI's own processes before the run.
ps w | grep -E 'MinUI|nextui|trimui_inputd|keymon|audiomon' | grep -v grep | sed 's/^ *[0-9]* //' | sort >"$OUT/nextui.before"

# A wedged fb0 hangs every opener for good (the vendor SDL opens it at init), so nothing is started
# while a process is stuck in the framebuffer driver; the host reports it.
wedged=""
for p in /proc/[0-9]*; do
  w="$(cat "$p/wchan" 2>/dev/null)"
  case "$w" in fb_open|lock_fb_info|fb_*) wedged="$wedged ${p#/proc/}:$w:$(cat "$p/comm" 2>/dev/null)";; esac
done
if [ -n "$wedged" ]; then
  echo "wedged:$wedged" >"$OUT/fb.status"
  exit 0
fi

# The smoke screenshot of the same screen (the app renders it itself, so it is the reference picture).
rm -f "$DATA/smoke/smoke.bmp" "$DATA/smoke/RESULT"
sh "$SD/Tools/tg5040/PicoTrackerSmoke.pak/launch.sh" >/dev/null 2>&1
cp "$DATA/smoke/smoke.bmp" "$OUT/smoke.bmp" 2>/dev/null
cp "$DATA/smoke/RESULT" "$OUT/smoke.result" 2>/dev/null
cp "$DATA/smoke/smoke.log" "$OUT/smoke.log" 2>/dev/null

# Virtual gamepad for the exit chord.
FIFO="$TOOLS/pad.fifo"
rm -f "$FIFO"; mkfifo "$FIFO"
chmod +x "$TOOLS/uinput_inject"
"$TOOLS/uinput_inject" "$FIFO" >"$OUT/inject.log" 2>&1 &
inject_pid=$!
i=0
while ! grep -q ready "$OUT/inject.log" 2>/dev/null && [ "$i" -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
grep -q ready "$OUT/inject.log" || say "virtual gamepad not created"
pad() { printf '%s\n' "$@" >"$FIFO"; }
export SDL_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1

# Start the app the way NextUI does: the pak's launch.sh, in the background.
sh "$PAK/launch.sh" >/dev/null 2>&1 &
app_pid=$!
sleep "$WAIT"

# CPU of the picotracker process over 5 s, from /proc (clock ticks of utime+stime).
pt_pid="$(ps w | grep '[b]in/picotracker' | grep -v 'sh ' | sed 's/^ *\([0-9]*\) .*/\1/' | head -n1)"
echo "${pt_pid:-none}" >"$OUT/pt.pid"
ticks() { awk '{print $14 + $15}' "/proc/$1/stat" 2>/dev/null; }
if [ -n "$pt_pid" ]; then
  t0="$(ticks "$pt_pid")"; sleep 5; t1="$(ticks "$pt_pid")"
  hz="$(getconf CLK_TCK 2>/dev/null || echo 100)"
  [ -n "$t0" ] && [ -n "$t1" ] && echo "$(( (t1 - t0) * 100 / (5 * hz) ))" >"$OUT/cpu.percent"
  grep -E 'VmRSS|Threads|State' "/proc/$pt_pid/status" >"$OUT/pt.status" 2>&1
fi

# fb0: geometry from sysfs (plain file reads), then the visible page, read by a background dd.
FB=/sys/class/graphics/fb0
{
  echo "virtual_size=$(cat $FB/virtual_size 2>&1)"; echo "stride=$(cat $FB/stride 2>&1)"
  echo "bpp=$(cat $FB/bits_per_pixel 2>&1)"; echo "pan=$(cat $FB/pan 2>&1)"; echo "modes=$(cat $FB/modes 2>&1)"
} >"$OUT/fb.info"
{
  stride="$(cat $FB/stride)"
  pan_y="$(cut -d, -f2 $FB/pan)"
  rm -f "$OUT/fb.raw"
  ( dd if=/dev/fb0 of="$OUT/fb.raw" bs="$stride" skip="$pan_y" count=720 >"$OUT/dd.log" 2>&1; echo done >"$OUT/dd.done" ) &
  rm -f "$OUT/dd.done"
  i=0
  while [ ! -f "$OUT/dd.done" ] && [ "$i" -lt 100 ]; do sleep 0.1; i=$((i + 1)); done
  if [ -f "$OUT/dd.done" ]; then echo ok >"$OUT/fb.status"; else echo "hung: dd on /dev/fb0 did not finish in 10 s" >"$OUT/fb.status"; fi
}
cp "$DATA/logs/picotracker.log" "$OUT/picotracker.log.live" 2>/dev/null

# The exit chord, repeated: the first one can arrive while the app is on another screen.
tries=0
while kill -0 "$app_pid" 2>/dev/null && [ "$tries" -lt 10 ]; do
  pad chord; tries=$((tries + 1)); sleep 2
done
if kill -0 "$app_pid" 2>/dev/null; then
  echo timeout >"$OUT/exit.status"
  killall picotracker 2>/dev/null; sleep 1; kill "$app_pid" 2>/dev/null
else
  wait "$app_pid"; echo "$?" >"$OUT/exit.status"
fi
pad quit; sleep 1; kill "$inject_pid" 2>/dev/null; rm -f "$FIFO"
cp "$DATA/logs/picotracker.log" "$OUT/picotracker.log" 2>/dev/null
ps w | grep -E 'MinUI|nextui|trimui_inputd|keymon|audiomon' | grep -v grep | sed 's/^ *[0-9]* //' | sort >"$OUT/nextui.after"
ps w | grep '[b]in/picotracker' >"$OUT/pt.after"
EOF
}

say "capturing on ${ssh_host} (smoke reference, then the live app, ${wait_s} s)"
device_script | rssh "sh -s -- ${wait_s} ${remote_tmp}/out ${remote_tmp}" >"${out_dir}/device.out" 2>&1 \
  || fail "device run failed (see ${out_dir}/device.out)"
rscp -r "${ssh_host}:${remote_tmp}/out/." "${out_dir}/" >/dev/null 2>&1 || fail "could not copy the results back"
rssh "rm -rf ${remote_tmp}" || true

# 4. Checks.
status="$(cat "${out_dir}/fb.status" 2>/dev/null || echo missing)"
if [[ "${status}" == ok && -s "${out_dir}/fb.raw" ]]; then
  info() { sed -n "s/^$1=//p" "${out_dir}/fb.info"; }
  stride="$(info stride)"
  vsize="$(info virtual_size)"
  width="${vsize%%,*}"
  smoke_args=()
  [[ -s "${out_dir}/smoke.bmp" ]] && smoke_args=(--smoke "${out_dir}/smoke.bmp")
  python3 -I "${project_root}/tests/trimui/display_check.py" "${out_dir}/fb.raw" "${out_dir}/panel.bmp" \
    --width "${width}" --height 720 --stride "${stride}" ${smoke_args[@]+"${smoke_args[@]}"} | tee "${out_dir}/picture.txt" \
    && ok "picture checks" || fail "picture checks (see ${out_dir}/picture.txt)"
else
  fail "no panel capture: fb0 ${status}"
fi

log="${out_dir}/picotracker.log"
if [[ -s "${log}" ]]; then
  driver="$(sed -n 's/^video driver: //p' "${log}" | head -n1)"
  case "${driver}" in
    "" | none | offscreen | dummy | evdev) fail "video driver '${driver:-missing}' draws nothing on the panel" ;;
    *) ok "video driver: ${driver}" ;;
  esac
  panel="$(sed -n 's/^display: trimui panel \([0-9]*x[0-9]*\).*/\1/p' "${log}" | head -n1)"
  [[ "${panel}" == 1280x720 ]] && ok "app sees the real panel size ${panel}" || fail "panel size in the log is '${panel:-missing}', expected 1280x720"
else
  fail "no app log"
fi

cpu="$(cat "${out_dir}/cpu.percent" 2>/dev/null || true)"
if [[ -n "${cpu}" ]]; then
  [[ "${cpu}" -le 150 ]] && ok "picotracker CPU ${cpu}% of one core over 5 s (4 cores)" || fail "picotracker CPU ${cpu}% of one core is not sane"
else
  fail "no CPU reading (picotracker pid: $(cat "${out_dir}/pt.pid" 2>/dev/null || echo unknown))"
fi

[[ "$(cat "${out_dir}/exit.status" 2>/dev/null)" == 0 ]] && ok "exit chord ended the app with status 0" \
  || fail "exit status after the chord: $(cat "${out_dir}/exit.status" 2>/dev/null || echo missing)"
if [[ ! -f "${out_dir}/pt.after" ]]; then fail "no process listing after the run"
elif [[ -s "${out_dir}/pt.after" ]]; then fail "picotracker still running: $(cat "${out_dir}/pt.after")"
else ok "picotracker is not running afterwards"; fi
if [[ -s "${out_dir}/nextui.before" ]] && diff "${out_dir}/nextui.before" "${out_dir}/nextui.after" >"${out_dir}/nextui.diff"; then
  ok "NextUI processes unchanged ($(wc -l <"${out_dir}/nextui.after" | tr -d ' ') running)"
else
  fail "NextUI processes changed or were not found (see ${out_dir}/nextui.diff)"
fi

echo "results: ${out_dir}"
if [[ ${failures} -gt 0 ]]; then echo "display check: ${failures} failure(s)" >&2; exit 1; fi
say "display check passed"
