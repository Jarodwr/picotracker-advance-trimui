#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Cross-builds the wrapper for the Trimui Smart Pro (aarch64 Linux) inside the
# image from docker/trimui/Dockerfile and prints the ELF's glibc and library
# requirements. Output: ${PICOTRACKER_TRIMUI_BUILD_DIR:-build/trimui}/picotracker_desktop
#
# PICOTRACKER_TRIMUI_SMOKE=1 also boots the result in an Ubuntu 16.04 arm64 container, then runs
# tests/trimui/emulated_smoke.sh on the packaged pak (launcher, seeding, log rotation, scripted input).
# PICOTRACKER_UPSTREAM_ROOT overrides the upstream checkout (default: submodule).
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
image=picotracker-trimui-build:1

if [[ "${PICOTRACKER_IN_CONTAINER:-}" != 1 ]]; then
  upstream="${PICOTRACKER_UPSTREAM_ROOT:-${project_root}/upstream/advance-firmware}"
  # A worktree has an empty submodule directory; fall back to the main checkout.
  if [[ ! -e "${upstream}/sources/Application/Model/Song.cpp" ]]; then
    common="$(git -C "${project_root}" rev-parse --path-format=absolute --git-common-dir)"
    upstream="$(dirname "${common}")/upstream/advance-firmware"
  fi
  build_dir="${PICOTRACKER_TRIMUI_BUILD_DIR:-${project_root}/build/trimui}"
  mkdir -p "${build_dir}"
  docker build -q -t "${image}" "${project_root}/docker/trimui" >/dev/null
  docker run --rm \
    -v "${project_root}:/src:ro" -v "${upstream}:/up:ro" -v "${build_dir}:/build" \
    -e PICOTRACKER_IN_CONTAINER=1 -e PICOTRACKER_UPSTREAM_ROOT=/up \
    "${image}" bash /src/scripts/build-trimui.sh
  # Optional: boot the binary on a real glibc 2.23 userland (Ubuntu 16.04
  # arm64). Needs an arm64 Docker host or qemu binfmt.
  if [[ "${PICOTRACKER_TRIMUI_SMOKE:-}" == 1 ]]; then
    # The binary links SDL2 dynamically; off the device the link stub stands in for the vendor library.
    docker run --rm --platform linux/arm64 -v "${build_dir}:/b:ro" ubuntu:16.04 \
      env SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy LD_LIBRARY_PATH=/b/sdl-stub \
      /b/picotracker_desktop --smoke --data-dir /tmp/smoke-data
    echo "Trimui smoke run on glibc 2.23 passed"
    "${project_root}/tests/trimui/emulated_smoke.sh" --binary "${build_dir}/picotracker_desktop"
  fi
  exit 0
fi

cmake -S /src -B /build/obj -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/src/cmake/toolchains/trimui-smart-pro.cmake \
  -DPICOTRACKER_UPSTREAM_ROOT=/up
cmake --build /build/obj --target picotracker_desktop
cp /build/obj/picotracker_desktop /build/picotracker_desktop
# The SDL2 link stub, for the emulated smoke only (the device uses its vendor SDL2).
rm -rf /build/sdl-stub && cp -a /opt/trimui-sdl-stub /build/sdl-stub

echo "== file"; file /build/picotracker_desktop
echo "== needed libraries"; readelf -d /build/picotracker_desktop | grep NEEDED
echo "== glibc symbol versions"
readelf -V /build/picotracker_desktop | grep -o 'GLIBC_[0-9.]*' | sort -uV
echo "Trimui build done: /build/picotracker_desktop"
