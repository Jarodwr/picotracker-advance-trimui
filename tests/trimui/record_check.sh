#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# shellcheck disable=SC2015,SC2029 # A && ok || fail is intentional; the remote command is built on this side

# Recording check for the Trimui Smart Pro: drives the installed pak over ssh with a virtual gamepad,
# records a few seconds on the Record screen and checks the WAV and what the run left behind.
#
# usage: record_check.sh --ssh USER@HOST[:SD_ROOT] [--source mic|line] [--seconds N] [--label NAME]
#                        [--binary PATH] [--out-dir DIR] [--skip-deploy]
#        record_check.sh --emulated [--binary PATH]    runs tests/trimui/emulated_smoke.sh instead
#   --source   mic (default): RecordView source Mic, a mono file. line: Line In, a stereo file.
#              The Smart Pro has one built-in mic on MIC1 (left channel); with a 3.5 mm headset plugged
#              in the jack's mic replaces it. Plug (or not) before running and name the run with --label
#              (for example "headset" or "no-headset"); the script cannot tell which one is present.
#   --seconds  how long the recording runs (default 5).
#   --skip-deploy  reuse the pak already on the card (default: scripts/deploy-trimui.sh first).
#
# Checks, each printed as ok/FAIL: the app exits on the chord; REC01.wav exists under the data root's
# recordings/, loads, is 16-bit 44100 Hz, about the requested length, has signal (non-zero RMS) and
# follows the owner's mono policy (mono file, or stereo with L copied to R); the ALSA mixer is the
# same before and after; the app log has no ALSA underrun or overrun between the start and the stop
# of the recording (playback is running while the capture is open). Nothing is left on the card
# except the pak and the data root; the WAV and the logs are copied to the out dir.
#
# Needs: ssh and scp (set TRIMUI_SSH_PORT if not 22; password prompts need SSH_ASKPASS or a key),
# docker for the on-device gamepad injector (first run only), python3 on this machine.
# The device needs /dev/uinput (it has it) and no Python.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ssh_target=""
emulated=0
source_kind=mic
seconds=5
label=""
binary="${project_root}/build/trimui/picotracker_desktop"
out_dir=""
skip_deploy=0
remote_tmp=/tmp/picotracker-record-check

die() { echo "record_check: $*" >&2; exit 1; }
say() { echo "[record-check] $*"; }
failures=0
fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
ok() { echo "ok: $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ssh) [[ $# -ge 2 ]] || die "--ssh needs USER@HOST"; ssh_target="$2"; shift ;;
    --emulated) emulated=1 ;;
    --source) [[ $# -ge 2 ]] || die "--source needs mic or line"; source_kind="$2"; shift ;;
    --seconds) [[ $# -ge 2 ]] || die "--seconds needs a number"; seconds="$2"; shift ;;
    --label) [[ $# -ge 2 ]] || die "--label needs a name"; label="$2"; shift ;;
    --binary) [[ $# -ge 2 ]] || die "--binary needs a path"; binary="$2"; shift ;;
    --out-dir) [[ $# -ge 2 ]] || die "--out-dir needs a directory"; out_dir="$2"; shift ;;
    --skip-deploy) skip_deploy=1 ;;
    -h | --help) sed -n '3,26p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done

if [[ ${emulated} -eq 1 ]]; then
  [[ -z "${ssh_target}" ]] || die "give either --emulated or --ssh"
  exec "${project_root}/tests/trimui/emulated_smoke.sh" --binary "${binary}"
fi
[[ -n "${ssh_target}" ]] || die "give --ssh USER@HOST[:SD_ROOT] or --emulated"
case "${source_kind}" in mic) source_value=2 ;; line) source_value=1 ;; *) die "--source is mic or line" ;; esac
[[ "${seconds}" =~ ^[0-9]+$ && ${seconds} -ge 1 ]] || die "--seconds needs a whole number >= 1"

ssh_host="${ssh_target%%:*}"
sd_root="/mnt/SDCARD"
[[ "${ssh_target}" == *:* ]] && sd_root="${ssh_target#*:}"
[[ "${sd_root}" == /mnt/SDCARD ]] || die "record_device.sh assumes the SD card at /mnt/SDCARD"
name="${label:-${source_kind}}"
[[ -n "${out_dir}" ]] || out_dir="${project_root}/build/trimui-record/$(date +%Y%m%d-%H%M%S)-${name}"
mkdir -p "${out_dir}"

port_args_ssh=()
port_args_scp=()
if [[ -n "${TRIMUI_SSH_PORT:-}" ]]; then port_args_ssh=(-p "${TRIMUI_SSH_PORT}"); port_args_scp=(-P "${TRIMUI_SSH_PORT}"); fi
rssh() { ssh ${port_args_ssh[@]+"${port_args_ssh[@]}"} "${ssh_host}" "$@"; }
rscp() { scp ${port_args_scp[@]+"${port_args_scp[@]}"} "$@"; }

# 1. The gamepad injector for the device (aarch64, built in the toolchain image, cached).
injector="${project_root}/build/trimui-record/uinput_inject"
if [[ ! -x "${injector}" || "${project_root}/tests/trimui/uinput_inject.c" -nt "${injector}" ]]; then
  say "building the on-device gamepad injector"
  mkdir -p "$(dirname "${injector}")"
  docker build -q -t picotracker-trimui-build:1 "${project_root}/docker/trimui" >/dev/null
  docker run --rm -v "${project_root}:/src:ro" -v "$(dirname "${injector}"):/out" picotracker-trimui-build:1 \
    trimui-cc -O2 -Wall -o /out/uinput_inject /src/tests/trimui/uinput_inject.c
fi

# 2. Deploy the pak, then put the helpers in the device's /tmp (not on the SD card).
if [[ ${skip_deploy} -eq 0 ]]; then
  say "deploying the pak"
  "${project_root}/scripts/deploy-trimui.sh" --binary "${binary}" --ssh "${ssh_target}" >"${out_dir}/deploy.log" 2>&1 \
    || { cat "${out_dir}/deploy.log" >&2; die "deploy failed"; }
fi
rssh "rm -rf ${remote_tmp} && mkdir -p ${remote_tmp}"
rscp "${injector}" "${project_root}/tests/trimui/record_device.sh" "${ssh_host}:${remote_tmp}/"

# 3. The run itself.
say "recording ${seconds} s from ${source_kind} (${name}) on ${ssh_host}"
rssh "chmod +x ${remote_tmp}/uinput_inject; sh ${remote_tmp}/record_device.sh ${seconds} ${remote_tmp}/out ${remote_tmp} ${source_value}" \
  >"${out_dir}/device.log" 2>&1 || { cat "${out_dir}/device.log" >&2; fail "device run failed"; }
rscp -r "${ssh_host}:${remote_tmp}/out/." "${out_dir}/" >/dev/null 2>&1 || fail "could not copy the results back"

# 4. Leave the device as found: helpers gone, the WAV (a file this run made) removed from the data root.
rssh "rm -rf ${remote_tmp}; rm -f '${sd_root}/.userdata/tg5040/PicoTracker/recordings/REC01.wav'; sync" || true

# 5. Checks.
[[ "$(cat "${out_dir}/exit.status" 2>/dev/null)" == 0 ]] && ok "app exited 0 on the chord" \
  || fail "app exit status: $(cat "${out_dir}/exit.status" 2>/dev/null || echo missing)"

if [[ -f "${out_dir}/REC01.wav" ]]; then
  ok "REC01.wav appeared under the data root (recordings/)"
  python3 -I "${project_root}/tests/trimui/wav_check.py" "${out_dir}/REC01.wav" "${seconds}" | tee "${out_dir}/wav.txt" \
    && ok "WAV loads, 44100 Hz, expected length, signal, mono policy" || fail "WAV check (see ${out_dir}/wav.txt)"
else
  fail "REC01.wav was not created"
fi

if [[ -s "${out_dir}/mixer.before" ]]; then
  if cmp -s "${out_dir}/mixer.before" "${out_dir}/mixer.after"; then ok "ALSA mixer identical before and after"
  else diff "${out_dir}/mixer.before" "${out_dir}/mixer.after" >&2 || true; fail "ALSA mixer differs after the run"; fi
else
  fail "no mixer snapshot (amixer missing?)"
fi

if [[ -f "${out_dir}/picotracker.log" && -f "${out_dir}/log.rec_start" && -f "${out_dir}/log.rec_end" ]]; then
  start="$(cat "${out_dir}/log.rec_start")"
  end="$(cat "${out_dir}/log.rec_end")"
  window="$(tail -c +"$((start + 1))" "${out_dir}/picotracker.log" | head -c "$((end - start))")"
  during="$(printf '%s\n' "${window}" | grep -ci 'underrun\|overrun\|xrun' || true)"
  total="$(grep -ci 'underrun\|overrun\|xrun' "${out_dir}/picotracker.log" || true)"
  echo "xruns in the log: ${total} in the whole run, ${during} while the capture was recording"
  [[ "${during}" == 0 ]] && ok "no underrun/overrun while playback and capture ran together" \
    || fail "${during} underrun/overrun line(s) during the recording"
else
  fail "no app log or log offsets to look for underruns"
fi

echo "results: ${out_dir}"
if [[ ${failures} -gt 0 ]]; then echo "record check: ${failures} failure(s)" >&2; exit 1; fi
say "record check passed (${name})"
