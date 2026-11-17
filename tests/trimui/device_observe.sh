#!/usr/bin/env bash
# shellcheck disable=SC2029,SC2015,SC2016 # remote commands are built on this side
# Observe a person-launched run of the PicoTracker pak on the Trimui (planning/findings/trimui-acceptance.md).
# An app started over SSH does not get the panel while NextUI is drawing, so the display, long-run and chord
# items are done by a person: start the pak from NextUI's menu when this script says so, look at the panel,
# leave with the exit chord. This script only takes the numbers around that run:
#   before/after   ALSA mixer snapshot and NextUI processes; OOM lines in dmesg
#   during         RSS, peak RSS, MemAvailable and swap every 15 s, from launch until the app exits
#   after          app log: video driver line, underrun/overrun/xrun count
# Starts nothing on the device and writes nothing outside /tmp/picotracker-observe there.
#
# usage: device_observe.sh [--ssh USER@HOST] [--wait SECONDS] [--max-minutes N] [--out-dir DIR]
#   --ssh defaults to $TRIMUI_SSH_USER@$TRIMUI_SSH_HOST (user root); auth as in device_acceptance.sh.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ssh_target=""
wait_s=120
max_min=10
out_dir=""
die() { echo "device_observe: $*" >&2; exit 1; }
while [[ $# -gt 0 ]]; do
  case "$1" in
    --ssh) [[ $# -ge 2 ]] || die "--ssh needs USER@HOST"; ssh_target="$2"; shift ;;
    --wait) [[ $# -ge 2 ]] || die "--wait needs seconds"; wait_s="$2"; shift ;;
    --max-minutes) [[ $# -ge 2 ]] || die "--max-minutes needs a number"; max_min="$2"; shift ;;
    --out-dir) [[ $# -ge 2 ]] || die "--out-dir needs a directory"; out_dir="$2"; shift ;;
    -h | --help) sed -n '3,15p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done
[[ -n "${ssh_target}" ]] || ssh_target="${TRIMUI_SSH_USER:-root}@${TRIMUI_SSH_HOST:?set TRIMUI_SSH_HOST or give --ssh}"
[[ "${wait_s}" =~ ^[0-9]+$ && "${max_min}" =~ ^[0-9]+$ ]] || die "--wait and --max-minutes need whole numbers"
ssh_host="${ssh_target%%:*}"
[[ -n "${out_dir}" ]] || out_dir="${project_root}/build/trimui-observe/$(date +%Y%m%d-%H%M%S)"
mkdir -p "${out_dir}"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
ssh_opts=(-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 -o LogLevel=ERROR -o ServerAliveInterval=10 -o ServerAliveCountMax=3)
if [[ -n "${TRIMUI_SSH_PASSWORD:-}" ]]; then
  printf '#!/bin/sh\necho "$TRIMUI_SSH_PASSWORD"\n' >"${work}/askpass"
  chmod +x "${work}/askpass"
  export SSH_ASKPASS="${work}/askpass" SSH_ASKPASS_REQUIRE=force DISPLAY="${DISPLAY:-none}"
  ssh_opts+=(-o PubkeyAuthentication=no)
fi
if [[ -n "${TRIMUI_SSH_PORT:-}" ]]; then ssh_opts+=(-p "${TRIMUI_SSH_PORT}"); fi
rssh() { ssh "${ssh_opts[@]}" "${ssh_host}" "$@"; }

device_script() {
  cat <<'EOF'
WAIT="$1"; MAXS="$2"
DATA=/mnt/SDCARD/.userdata/tg5040/PicoTracker
kb() { awk -v k="$1" '$1 == k":" { print $2 }' "${2:-/proc/meminfo}"; }
oomn() { dmesg | grep -c -i -E 'out of memory|oom-kill|Killed process'; }
nextui() { ps w | grep -E 'MinUI|nextui|trimui_inputd|keymon|audiomon' | grep -v grep | sed 's/^ *[0-9]* //' | sort; }
echo "## mixer before"; amixer -c 0 contents 2>/dev/null
echo "## nextui before"; nextui
oom0=$(oomn)
echo "## waiting"
t=0; pid=""
while [ "$t" -lt "$WAIT" ]; do
  pid=$(pidof picotracker | cut -d' ' -f1); [ -n "$pid" ] && break
  sleep 1; t=$((t + 1))
done
[ -n "$pid" ] || { echo "## error: picotracker was not started within ${WAIT} s"; exit 0; }
echo "## running pid $pid"
echo "## samples (s, VmRSS kB, VmHWM kB, MemAvailable kB, swap used kB)"
t=0
while [ -d /proc/$pid ] && [ "$t" -lt "$MAXS" ]; do
  echo "$t	$(kb VmRSS /proc/$pid/status)	$(kb VmHWM /proc/$pid/status)	$(kb MemAvailable)	$(( $(kb SwapTotal) - $(kb SwapFree) ))"
  sleep 15; t=$((t + 15))
done
if [ -d /proc/$pid ]; then echo "## still running after ${MAXS} s"; else echo "## app exited after about ${t} s"; fi
sleep 2
echo "## oom lines: $(( $(oomn) - oom0 ))"
echo "## mixer after"; amixer -c 0 contents 2>/dev/null
echo "## nextui after"; nextui
echo "## log"; cat "$DATA/logs/picotracker.log" 2>/dev/null
EOF
}

echo "[observe] now start PicoTracker from NextUI's menu on the device (waiting up to ${wait_s} s), look at the panel, leave with the exit chord"
device_script | rssh "sh -s -- ${wait_s} $((max_min * 60))" >"${out_dir}/observe.txt"

sec() { awk -v s="## $1" '/^## /{p = (index($0, s) == 1)} p && !/^## /' "${out_dir}/observe.txt"; }
if grep -q '^## error' "${out_dir}/observe.txt"; then grep '^## error' "${out_dir}/observe.txt" >&2; exit 2; fi
sec "mixer before" >"${out_dir}/mixer.before"; sec "mixer after" >"${out_dir}/mixer.after"
sec "nextui before" >"${out_dir}/nextui.before"; sec "nextui after" >"${out_dir}/nextui.after"
sec "log" >"${out_dir}/app.log"; sec "samples" >"${out_dir}/samples"
failures=0
pass() { echo "PASS $1: $2"; }
fail() { echo "FAIL $1: $2" >&2; failures=$((failures + 1)); }
grep -q 'video driver:' "${out_dir}/app.log" && ! grep -q 'video driver: \(offscreen\|dummy\)' "${out_dir}/app.log" \
  && pass video "$(grep -m1 'video driver:' "${out_dir}/app.log")" || fail video "no real video driver line"
n="$(grep -ci 'underrun\|overrun\|xrun' "${out_dir}/app.log" || true)"
[[ "${n}" == 0 ]] && pass audio "no underrun/overrun/xrun line" || fail audio "${n} underrun/overrun/xrun line(s)"
awk -F'\t' 'NF >= 5 && $1 ~ /^[0-9]+$/ { if ($3 > m) m = $3; if (!seen || $4 < l) l = $4; seen = 1; if ($5 > s) s = $5 } END { if (!seen) exit 1; printf "peak RSS %d MB, lowest MemAvailable %d MB, max swap %d KB\n", m / 1024, l / 1024, s }' \
  "${out_dir}/samples" || echo "no memory samples (the app ran under 15 s)"
grep '^## oom' "${out_dir}/observe.txt" | grep -q ': 0$' && pass oom "no new OOM line in dmesg" || fail oom "OOM line(s) in dmesg"
cmp -s "${out_dir}/mixer.before" "${out_dir}/mixer.after" && pass mixer "ALSA mixer identical" || fail mixer "mixer differs (diff in ${out_dir}/mixer.*)"
cmp -s "${out_dir}/nextui.before" "${out_dir}/nextui.after" && pass nextui "NextUI processes unchanged" || fail nextui "NextUI processes changed"
grep '^## app exited\|^## still running' "${out_dir}/observe.txt"
echo "results: ${out_dir}"
[[ ${failures} -eq 0 ]] || exit 1
