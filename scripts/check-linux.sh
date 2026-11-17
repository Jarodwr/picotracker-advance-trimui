#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Builds and checks the wrapper natively on Linux. On a non-Linux host (e.g.
# macOS) it runs the same steps in an Ubuntu 24.04 container of the host's
# architecture, so Apple silicon exercises Linux aarch64.
#
# PICOTRACKER_UPSTREAM_ROOT overrides the upstream checkout (default: the
# submodule), e.g. to validate patches/0001 in a disposable copy.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ "$(uname -s)" != "Linux" ]]; then
  upstream="${PICOTRACKER_UPSTREAM_ROOT:-${project_root}/upstream/advance-firmware}"
  # A worktree has an empty submodule directory; fall back to the main checkout.
  if [[ ! -e "${upstream}/sources/Application/Model/Song.cpp" ]]; then
    common="$(git -C "${project_root}" rev-parse --path-format=absolute --git-common-dir)"
    upstream="$(dirname "${common}")/upstream/advance-firmware"
  fi
  exec docker run --rm \
    -v "${project_root}:/src:ro" -v "${upstream}:/up:ro" \
    -v "${PICOTRACKER_LINUX_BUILD_DIR:-/tmp/picotracker-linux-build}:/build" \
    -e PICOTRACKER_IN_CONTAINER=1 -e PICOTRACKER_UPSTREAM_ROOT=/up \
    ubuntu:24.04 bash /src/scripts/check-linux.sh
fi

if [[ "${PICOTRACKER_IN_CONTAINER:-}" == 1 ]]; then
  apt-get update -qq
  apt-get install -y -qq g++ cmake ninja-build libsdl2-dev python3 >/dev/null
  build_dir=/build
else
  build_dir="${project_root}/build/linux-check"
fi

cmake -S "${project_root}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  ${PICOTRACKER_UPSTREAM_ROOT:+-DPICOTRACKER_UPSTREAM_ROOT="${PICOTRACKER_UPSTREAM_ROOT}"}
cmake --build "${build_dir}"
"${build_dir}/host_filesystem_check"
"${build_dir}/host_input_map_check"
"${build_dir}/host_quit_dialog_check"
"${build_dir}/host_battery_check"
"${build_dir}/host_volume_check"
SDL_AUDIODRIVER=dummy "${build_dir}/host_audio_capture_check"
SDL_AUDIODRIVER=dummy "${build_dir}/host_record_check"
SDL_VIDEODRIVER=dummy "${build_dir}/host_event_check"
python3 "${project_root}/tests/compatibility_check.py" \
  "${build_dir}/picotracker_desktop"
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "${build_dir}/picotracker_desktop" \
  --smoke --data-dir "${build_dir}/smoke-data" \
  --screenshot "${build_dir}/first-firmware-view.bmp"

echo "Linux check passed ($(uname -m)). Screenshot: ${build_dir}/first-firmware-view.bmp"
