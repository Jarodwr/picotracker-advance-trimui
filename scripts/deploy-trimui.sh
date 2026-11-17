#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Installs a cross-built picotracker_desktop on a Trimui Smart Pro (NextUI, tg5040)
# using the pak layout of ADR 0017. The app pak is the one built by scripts/package-trimui.sh
# (single launch.sh, licences, share/), replaced wholesale. Two paks are written:
#   Tools/tg5040/PicoTracker.pak/       the packaged app
#   Tools/tg5040/PicoTrackerSmoke.pak/  launch.sh that boots --smoke and saves a log and a screenshot
#
# usage: deploy-trimui.sh [--dry-run] [--binary PATH] (--sd-root DIR | --ssh USER@HOST[:SD_ROOT])
#   --sd-root DIR   SD card mounted on this machine (e.g. /Volumes/NEXTUI). Files are copied directly.
#   --ssh TARGET    device reachable over ssh; SD_ROOT defaults to /mnt/SDCARD.
#   --binary PATH   default: build/trimui/picotracker_desktop (output of scripts/build-trimui.sh).
#   --dry-run       print every action and change nothing (the binary need not exist).
#
# Neither transport has been run against a device; see docs/trimui-first-run.md.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
binary="${project_root}/build/trimui/picotracker_desktop"
sd_root=""
ssh_target=""
dry_run=0
platform=tg5040
pak=PicoTracker.pak
smoke_pak=PicoTrackerSmoke.pak

die() { echo "deploy-trimui: $*" >&2; exit 1; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --dry-run) dry_run=1 ;;
    --binary) [[ $# -ge 2 ]] || die "--binary needs a path"; binary="$2"; shift ;;
    --sd-root) [[ $# -ge 2 ]] || die "--sd-root needs a directory"; sd_root="$2"; shift ;;
    --ssh) [[ $# -ge 2 ]] || die "--ssh needs USER@HOST"; ssh_target="$2"; shift ;;
    -h | --help) sed -n '2,16p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done

if [[ -n "${sd_root}" && -n "${ssh_target}" ]] || [[ -z "${sd_root}" && -z "${ssh_target}" ]]; then
  die "give exactly one of --sd-root or --ssh"
fi

# Tools/<platform>/ under the SD root, as seen from this machine and from the device.
ssh_host="${ssh_target%%:*}"
device_sd="/mnt/SDCARD"
[[ "${ssh_target}" == *:* ]] && device_sd="${ssh_target#*:}"
tools_dir="${sd_root}/Tools/${platform}"
[[ -n "${ssh_target}" ]] && tools_dir="${device_sd}/Tools/${platform}"

# Boots --smoke, keeps the log and the first frame in <data root>/smoke/ and writes
# smoke/RESULT (exit status). tests/trimui/smoke.sh reads these back.
smoke_launch_sh() {
  cat <<'EOF'
#!/bin/sh
# PicoTracker boot smoke for Trimui Smart Pro (tg5040)
PAK_DIR="$(cd "$(dirname "$0")" && pwd)"
SD_ROOT="$(cd "$PAK_DIR/../../.." && pwd)"
APP_DIR="$PAK_DIR/../PicoTracker.pak"
USERDATA="${USERDATA_PATH:-$SD_ROOT/.userdata/tg5040}"
DATA_DIR="$USERDATA/PicoTracker"
OUT="$DATA_DIR/smoke"
mkdir -p "$DATA_DIR/projects" "$DATA_DIR/samples" "$OUT"
rm -f "$OUT/RESULT" "$OUT/smoke.bmp"
export SDL_NOMOUSE=1
if [ -d "$APP_DIR/lib" ]; then
    export LD_LIBRARY_PATH="$APP_DIR/lib:${LD_LIBRARY_PATH:-}"
fi
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:+$LD_LIBRARY_PATH:}/usr/trimui/lib"
export HOME="${HOME:-$USERDATA}"
{
    echo "date: $(date)"
    echo "uname: $(uname -a)"
    echo "sdl env: VIDEODRIVER=${SDL_VIDEODRIVER:-unset} AUDIODRIVER=${SDL_AUDIODRIVER:-unset}"
    ls -l /lib/libc.so.6 /lib/ld-linux-aarch64.so.1 2>&1
} >"$OUT/smoke.log"
cd "$APP_DIR" || exit 1
"$APP_DIR/bin/picotracker" --smoke --data-dir "$DATA_DIR" --sample-memory-mib 48 \
    --screenshot "$OUT/smoke.bmp" >>"$OUT/smoke.log" 2>&1
echo "$?" >"$OUT/RESULT"
EOF
}

say() { echo "[deploy-trimui] $*"; }

run() {
  echo "+ $*"
  if [[ ${dry_run} -eq 0 ]]; then "$@"; fi
}

if [[ ${dry_run} -eq 0 && ! -f "${binary}" ]]; then
  die "binary not found: ${binary} (run scripts/build-trimui.sh)"
fi
if [[ ${dry_run} -eq 1 && ! -f "${binary}" ]]; then
  say "note: ${binary} does not exist; fine for --dry-run"
fi

stage="$(mktemp -d)"
trap 'rm -rf "${stage}"' EXIT
package_binary="${binary}"
if [[ ! -f "${binary}" ]]; then
  # --dry-run without a build: stage the layout around an empty placeholder binary.
  package_binary="${stage}/placeholder-binary"
  : >"${package_binary}"
fi
# PICOTRACKER_UPSTREAM_ROOT lets a worktree (empty submodule directory) use another firmware checkout.
upstream_args=()
[[ -z "${PICOTRACKER_UPSTREAM_ROOT:-}" ]] || upstream_args=(--upstream "${PICOTRACKER_UPSTREAM_ROOT}")
"${project_root}/scripts/package-trimui.sh" --binary "${package_binary}" --stage-only "${stage}/tree" \
  ${upstream_args[@]+"${upstream_args[@]}"} >/dev/null
mkdir -p "${stage}/tree/Tools/${platform}/${smoke_pak}"
smoke_launch_sh >"${stage}/tree/Tools/${platform}/${smoke_pak}/launch.sh"
chmod 0755 "${stage}/tree/Tools/${platform}/${smoke_pak}/launch.sh"
src_app="${stage}/tree/Tools/${platform}/${pak}"
src_smoke="${stage}/tree/Tools/${platform}/${smoke_pak}"
say "staged the pak layout in a temporary directory"
if [[ ${dry_run} -eq 1 ]]; then
  say "layout that would be installed under ${tools_dir}:"
  (cd "${stage}/tree/Tools/${platform}" && find . -mindepth 1 | LC_ALL=C sort | sed -e 's#^\./#    #')
  say "launch.sh that would be installed in ${pak}:"
  sed 's/^/    | /' "${src_app}/launch.sh"
  say "launch.sh that would be installed in ${smoke_pak}:"
  sed 's/^/    | /' "${src_smoke}/launch.sh"
fi

if [[ -n "${sd_root}" ]]; then
  say "mode: SD card at ${sd_root}"
  if [[ ${dry_run} -eq 0 && ! -d "${sd_root}" ]]; then die "SD root is not a directory: ${sd_root}"; fi
  run mkdir -p "${tools_dir}"
  run rm -rf "${tools_dir:?}/${pak}" "${tools_dir:?}/${smoke_pak}"
  run cp -R "${src_app}" "${tools_dir}/${pak}"
  run cp -R "${src_smoke}" "${tools_dir}/${smoke_pak}"
  # FAT has no mode bits, so this may be a no-op; NextUI may run launch.sh with sh.
  run chmod 0755 "${tools_dir}/${pak}/bin/picotracker" "${tools_dir}/${pak}/launch.sh" \
    "${tools_dir}/${smoke_pak}/launch.sh" || say "chmod failed (expected on FAT); continuing"
  run sync
  say "eject the card before removing it"
else
  say "mode: ssh ${ssh_host}, SD root ${device_sd}"
  run ssh "${ssh_host}" "rm -rf '${tools_dir}/${pak}' '${tools_dir}/${smoke_pak}' && mkdir -p '${tools_dir}/${pak}' '${tools_dir}/${smoke_pak}'"
  # Copy the contents into directories that exist: newer scp (SFTP mode) refuses a missing destination.
  run scp -rp "${src_app}/." "${ssh_host}:${tools_dir}/${pak}/"
  run scp -rp "${src_smoke}/." "${ssh_host}:${tools_dir}/${smoke_pak}/"
  run ssh "${ssh_host}" "chmod 0755 '${tools_dir}/${pak}/bin/picotracker' '${tools_dir}/${pak}/launch.sh' '${tools_dir}/${smoke_pak}/launch.sh'; sync"
fi

if [[ ${dry_run} -eq 1 ]]; then say "dry run: nothing was changed"; else say "installed"; fi
