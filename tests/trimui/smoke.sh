#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# On-device boot smoke for the Trimui Smart Pro. Run scripts/deploy-trimui.sh first.
# The device runs `picotracker --smoke --screenshot`; this script keeps the log and the
# screenshot under <out-dir> (default: build/trimui-smoke/<timestamp>) and fails unless the
# run exited 0, produced a non-empty screenshot, and logged a 'video driver:' line naming a real
# driver. A missing line, or offscreen/dummy/none, fails: those draw nothing on the panel, which is
# how an earlier build passed while the screen stayed black.
#
# usage: smoke.sh [--dry-run] [--allow-headless] [--out-dir DIR] (--ssh USER@HOST[:SD_ROOT] | --sd-root DIR)
#   --ssh TARGET    run the smoke pak's launch.sh on the device, then copy the results back.
#   --sd-root DIR   collect results from a mounted SD card after you launched
#                   "PicoTrackerSmoke" from the NextUI Tools list on the device.
#   --allow-headless  accept an offscreen/dummy driver (emulated runs only; never on the device).
#   --dry-run       print every action and run nothing.
#
# Not run against a device; see docs/trimui-first-run.md.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
out_dir=""
sd_root=""
ssh_target=""
dry_run=0
allow_headless=0
platform=tg5040

die() { echo "smoke: $*" >&2; exit 1; }
say() { echo "[trimui-smoke] $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run) dry_run=1 ;;
    --allow-headless) allow_headless=1 ;;
    --out-dir) [[ $# -ge 2 ]] || die "--out-dir needs a directory"; out_dir="$2"; shift ;;
    --sd-root) [[ $# -ge 2 ]] || die "--sd-root needs a directory"; sd_root="$2"; shift ;;
    --ssh) [[ $# -ge 2 ]] || die "--ssh needs USER@HOST"; ssh_target="$2"; shift ;;
    -h | --help) sed -n '2,15p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done

if [[ -n "${sd_root}" && -n "${ssh_target}" ]] || [[ -z "${sd_root}" && -z "${ssh_target}" ]]; then
  die "give exactly one of --sd-root or --ssh"
fi
[[ -n "${out_dir}" ]] || out_dir="${project_root}/build/trimui-smoke/$(date +%Y%m%d-%H%M%S)"

run() {
  echo "+ $*"
  if [[ ${dry_run} -eq 0 ]]; then "$@"; fi
}

if [[ -n "${ssh_target}" ]]; then
  ssh_host="${ssh_target%%:*}"
  device_sd="/mnt/SDCARD"
  [[ "${ssh_target}" == *:* ]] && device_sd="${ssh_target#*:}"
  smoke_dir="${device_sd}/.userdata/${platform}/PicoTracker/smoke"
  say "mode: ssh ${ssh_host}"
  run mkdir -p "${out_dir}"
  # The smoke launcher always exits 0 and records the app's status in RESULT.
  run ssh "${ssh_host}" "sh '${device_sd}/Tools/${platform}/PicoTrackerSmoke.pak/launch.sh'"
  run scp "${ssh_host}:${smoke_dir}/smoke.log" "${ssh_host}:${smoke_dir}/smoke.bmp" \
    "${ssh_host}:${smoke_dir}/RESULT" "${out_dir}/"
else
  smoke_dir="${sd_root}/.userdata/${platform}/PicoTracker/smoke"
  say "mode: SD card at ${sd_root}"
  [[ ${dry_run} -eq 1 || -d "${smoke_dir}" ]] ||
    die "no ${smoke_dir}: launch PicoTrackerSmoke on the device first"
  run mkdir -p "${out_dir}"
  run cp "${smoke_dir}/smoke.log" "${smoke_dir}/smoke.bmp" "${smoke_dir}/RESULT" "${out_dir}/"
fi

if [[ ${dry_run} -eq 1 ]]; then
  say "would require: RESULT is 0, smoke.bmp is non-empty, the log's video driver is a real one, smoke.log is kept in ${out_dir}"
  say "dry run: nothing was run"
  exit 0
fi

[[ -f "${out_dir}/RESULT" ]] || die "no RESULT file: the smoke did not finish (see ${out_dir}/smoke.log)"
status="$(tr -d '[:space:]' <"${out_dir}/RESULT")"
if [[ "${status}" != 0 ]]; then
  echo "---- smoke.log (tail) ----"; tail -n 20 "${out_dir}/smoke.log" || true
  die "picotracker --smoke exited with status ${status}; log: ${out_dir}/smoke.log"
fi
[[ -s "${out_dir}/smoke.bmp" ]] || die "screenshot missing or empty: ${out_dir}/smoke.bmp"
# The app logs 'video driver: NAME' right after SDL init. offscreen and dummy render to memory only.
driver_line="$(grep -a -m1 '^video driver: ' "${out_dir}/smoke.log" || true)"
[[ -n "${driver_line}" ]] || die "no 'video driver:' line in ${out_dir}/smoke.log"
driver="${driver_line#video driver: }"
case "${driver}" in
  offscreen | dummy | none | "")
    if [[ ${allow_headless} -eq 1 ]]; then say "video driver ${driver} accepted (--allow-headless)"
    else die "video driver is '${driver}': nothing reaches the panel (log: ${out_dir}/smoke.log)"; fi ;;
  *) say "video driver: ${driver}" ;;
esac
say "PASS: exit 0, screenshot $(wc -c <"${out_dir}/smoke.bmp" | tr -d ' ') bytes"
say "results in ${out_dir}; record them in docs/trimui-first-run.md step 10"
