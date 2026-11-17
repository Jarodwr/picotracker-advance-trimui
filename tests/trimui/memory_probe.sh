#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# On-device sample-memory probe for the Trimui Smart Pro (planning/findings/trimui-sample-memory.md).
# Generates 8 MiB WAVs under the app's samples folder, starts the installed pak's binary once per
# --sample-memory-mib budget with enough --sample-probe files to fill the pool and then some, lets it
# run with audio open, and records MemAvailable, the app's RSS and peak RSS, swap use, new OOM lines
# in dmesg and audio errors in the app log. NextUI keeps running throughout.
#
# usage: memory_probe.sh [--budgets "48 128 256 384"] [--settle SECONDS] [--keep-wavs]
# Environment: TRIMUI_SSH_HOST, TRIMUI_SSH_PORT (22), TRIMUI_SSH_USER (root), TRIMUI_SSH_PASSWORD
#   (optional; used through a throwaway SSH_ASKPASS helper, never written anywhere else),
#   TRIMUI_SD_ROOT (/mnt/SDCARD). PicoTracker.pak must already be installed (scripts/deploy-trimui.sh).
# Writes only .userdata/tg5040/PicoTracker/samples/memprobe/ (removed on exit) and /tmp on the device.
# Prints one TSV row per budget, then a Markdown table.
set -euo pipefail

budgets="48 128 256 384"
settle=8
keep=0
die() { echo "memory_probe: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --budgets) [[ $# -ge 2 ]] || die "--budgets needs a list"; budgets="$2"; shift ;;
    --settle) [[ $# -ge 2 ]] || die "--settle needs seconds"; settle="$2"; shift ;;
    --keep-wavs) keep=1 ;;
    -h | --help) sed -n '2,13p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done

host="${TRIMUI_SSH_HOST:?set TRIMUI_SSH_HOST}"
port="${TRIMUI_SSH_PORT:-22}"
user="${TRIMUI_SSH_USER:-root}"
sd_root="${TRIMUI_SD_ROOT:-/mnt/SDCARD}"

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
ssh_opts=(-p "${port}" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8)
if [[ -n "${TRIMUI_SSH_PASSWORD:-}" ]]; then
  printf '#!/bin/sh\necho "$TRIMUI_SSH_PASSWORD"\n' >"${work}/askpass"
  chmod +x "${work}/askpass"
  export SSH_ASKPASS="${work}/askpass" SSH_ASKPASS_REQUIRE=force DISPLAY="${DISPLAY:-none}"
  ssh_opts+=(-o PubkeyAuthentication=no)
fi

# Device side: busybox sh. Prints "ROW<TAB>..." lines.
device_script() {
  cat <<'EOF'
SD="$1"; BUDGETS="$2"; SETTLE="$3"; KEEP="$4"
PAK="$SD/Tools/tg5040/PicoTracker.pak"
DATA="$SD/.userdata/tg5040/PicoTracker"
W="$DATA/samples/memprobe"
MIB8=$((8 * 1024 * 1024))
[ -x "$PAK/bin/picotracker" ] || { echo "no installed pak at $PAK" >&2; exit 1; }
mkdir -p "$W" "$DATA/projects" /tmp/pt
cleanup() { [ "$KEEP" = 1 ] || rm -rf "$W"; }
trap cleanup EXIT

le32() { printf "\\$(printf %03o $(($1 & 255)))\\$(printf %03o $((($1 >> 8) & 255)))\\$(printf %03o $((($1 >> 16) & 255)))\\$(printf %03o $((($1 >> 24) & 255)))"; }
kb() { awk -v k="$1" '$1 == k":" { print $2 }' "${2:-/proc/meminfo}"; }

# One 8 MiB mono 16-bit 44.1 kHz WAV; later copies are cp'd (probes must have distinct paths).
{ printf RIFF; le32 $((MIB8 + 36)); printf 'WAVEfmt '; le32 16; printf '\001\000\001\000'
  le32 44100; le32 88200; printf '\002\000\020\000'; printf data; le32 $MIB8
  head -c $MIB8 /dev/zero; } > "$W/s00.wav"
max=0; for b in $BUDGETS; do [ "$b" -gt "$max" ] && max=$b; done
need=$((max / 8 + 2)); i=1
while [ $i -lt $need ]; do cp "$W/s00.wav" "$W/s$(printf %02d $i).wav"; i=$((i + 1)); done
sync

export SDL_NOMOUSE=1 HOME=/tmp/pt
[ -d "$PAK/lib" ] && export LD_LIBRARY_PATH="$PAK/lib:${LD_LIBRARY_PATH:-}"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}/usr/trimui/lib"
cd "$PAK" || exit 1

for b in $BUDGETS; do
  count=$((b / 8 + 2))   # two more 8 MiB files than fit, so the budget is hit and refusal is seen
  args=""; i=0
  while [ $i -lt $count ]; do args="$args --sample-probe /samples/memprobe/s$(printf %02d $i).wav"; i=$((i + 1)); done
  log=/tmp/pt/probe-$b.log
  oom0=$(dmesg | grep -c -i -E 'out of memory|oom-kill|Killed process')
  sync; echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
  avail0=$(kb MemAvailable)
  # shellcheck disable=SC2086
  ./bin/picotracker --display trimui --data-dir "$DATA" --sample-memory-mib "$b" $args >"$log" 2>&1 &
  pid=$!
  # wait for every probe line (a probe at 384 MiB reads and converts ~400 MiB from SD)
  t=0
  while [ "$(grep -c '^sample probe' "$log")" -lt "$count" ] && kill -0 $pid 2>/dev/null && [ $t -lt 300 ]; do sleep 1; t=$((t + 1)); done
  sleep "$SETTLE"
  alive=no; kill -0 $pid 2>/dev/null && alive=yes
  rss=$(kb VmRSS /proc/$pid/status); hwm=$(kb VmHWM /proc/$pid/status)
  avail=$(kb MemAvailable); free=$(kb MemFree)
  swap=$(( $(kb SwapTotal) - $(kb SwapFree) ))
  loaded=$(grep '^sample probe' "$log" | grep -vc 'index=-1')
  refused=$(grep '^sample probe' "$log" | grep -c 'index=-1')
  left=$(grep '^sample probe' "$log" | tail -1 | sed 's/.*remaining=//')
  oom=$(( $(dmesg | grep -c -i -E 'out of memory|oom-kill|Killed process') - oom0 ))
  audio=$(grep -c -i -E 'underrun|xrun|alsa.*(error|fail)|audio.*(error|fail)|snd_pcm' "$log")
  kill $pid 2>/dev/null; t=0
  while kill -0 $pid 2>/dev/null && [ $t -lt 10 ]; do sleep 1; t=$((t + 1)); done
  kill -9 $pid 2>/dev/null; wait $pid 2>/dev/null
  echo "ROW	$b	$count	$loaded	$refused	$left	$avail0	$avail	$free	$rss	$hwm	$swap	$oom	$audio	$alive"
done
EOF
}

rows="$(device_script | ssh "${ssh_opts[@]}" "${user}@${host}" \
  "sh -s -- '${sd_root}' '${budgets}' '${settle}' '${keep}'")"
echo "${rows}" | grep -a '^ROW' | sed 's/^ROW\t//' | \
  { printf 'budget_mib\tprobes\tloaded\trefused\tpool_left_bytes\tmemavail0_kb\tmemavail_kb\tmemfree_kb\trss_kb\thwm_kb\tswap_kb\toom_kills\taudio_errors\talive\n'; cat; } | tee "${work}/rows.tsv"
echo
awk -F'\t' 'NR == 1 { print "| Budget MiB | Loaded / refused | MemAvailable before MB | MemAvailable with app MB | App RSS MB | App peak RSS MB | Swap KB | OOM kills | Audio errors | Alive |"
  print "|---|---|---|---|---|---|---|---|---|---|"; next }
  { printf "| %s | %s / %s | %d | %d | %d | %d | %s | %s | %s | %s |\n", $1, $3, $4, $6/1024, $7/1024, $9/1024, $10/1024, $11, $12, $13, $14 }' "${work}/rows.tsv"
