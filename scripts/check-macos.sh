#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This check runs on macOS." >&2
  exit 2
fi

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_root}/build/macos-check"
cmake_bin="${PICOTRACKER_CMAKE_BIN:-/opt/homebrew/bin/cmake}"
if [[ ! -x "${cmake_bin}" ]]; then
  cmake_bin="$(command -v cmake)"
fi

"${cmake_bin}" -S "${project_root}" -B "${build_dir}" -DCMAKE_BUILD_TYPE=Debug
"${cmake_bin}" --build "${build_dir}" -j 4
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
SDL_VIDEODRIVER=dummy "${build_dir}/picotracker_desktop" \
  --smoke --data-dir "${build_dir}/smoke-data" \
  --screenshot "${build_dir}/first-firmware-view.bmp"
sips -s format png "${build_dir}/first-firmware-view.bmp" \
  --out "${build_dir}/first-firmware-view.png" >/dev/null

# Audio and MIDI adapter checks (standalone; build lines are in each test header).
u="${project_root}/upstream/advance-firmware/sources"
cxx="${CXX:-c++}"
sdl_cflags="$(sdl2-config --cflags)"
sdl_libs="$(sdl2-config --libs)"
cd "${project_root}"
"${cxx}" -std=c++23 -DADV -Dstrlen=__builtin_strlen "-Dsection(x)=unused" \
  -Wno-deprecated-declarations -Iplatform/desktop -Iplatform/desktop/include \
  -I"$u" -I"$u/config" -I"$u/Externals/etl/include" -I"$u/Foundation/Variables" \
  ${sdl_cflags} tests/macos_audio_check.cpp \
  platform/desktop/SDLAudioDriver.cpp platform/desktop/HostTrace.cpp \
  "$u/Services/Audio/AudioDriver.cpp" "$u/Foundation/Observable.cpp" \
  -Wl,-dead_strip ${sdl_libs} -o "${build_dir}/macos_audio_check"
"${build_dir}/macos_audio_check" "${PICOTRACKER_AUDIO_SECONDS:-8}"
"${cxx}" -std=c++23 -DADV -Wno-deprecated-declarations -Itests/macos_midi_fake \
  -Iplatform/macos -I"$u" -I"$u/config" -I"$u/Externals/etl/include" \
  -I"$u/Foundation/Variables" tests/macos_midi_check.cpp \
  platform/macos/CoreMidiIn.cpp platform/macos/CoreMidiOut.cpp \
  "$u/Services/Midi/MidiInDevice.cpp" "$u/Services/Midi/MidiOutDevice.cpp" \
  "$u/Foundation/Observable.cpp" -framework CoreMIDI -framework CoreFoundation \
  -o "${build_dir}/macos_midi_check"
"${build_dir}/macos_midi_check"

# Bundle: build, copy away from the build directory, launch twice (the second
# run must find the first run's data), and confirm SDL loads from the bundle.
"${cmake_bin}" --build "${build_dir}" --target picotracker_macos_app
copy_dir="$(cd "$(mktemp -d "${TMPDIR:-/tmp}/picotracker-bundle.XXXXXX")" && pwd -P)"
trap 'rm -rf "${copy_dir}"' EXIT
cp -R "${build_dir}/bundle/picoTracker Advance Desktop.app" "${copy_dir}/"
app_bin="${copy_dir}/picoTracker Advance Desktop.app/Contents/MacOS/picotracker_desktop"
bundle_data="${copy_dir}/data"
for run in 1 2; do
  SDL_VIDEODRIVER=dummy "${app_bin}" --smoke --data-dir "${bundle_data}" \
    --screenshot "${copy_dir}/bundle-view.bmp"
done
test -s "${bundle_data}/.config.xml" || { echo "bundle did not keep its data" >&2; exit 1; }
libs="$(SDL_VIDEODRIVER=dummy DYLD_PRINT_LIBRARIES=1 "${app_bin}" --smoke \
  --data-dir "${bundle_data}" 2>&1 | grep -i 'libSDL' || true)"
echo "${libs}" | grep -q "${copy_dir}" || { echo "SDL did not load from the bundle" >&2; exit 1; }
if echo "${libs}" | grep -q "/opt/homebrew"; then
  echo "bundle loaded Homebrew SDL" >&2; exit 1
fi
codesign --verify --deep --strict "${copy_dir}/picoTracker Advance Desktop.app"

# Pinned upstream submodule must be clean and at the recorded commit.
if [[ -n "$(git -C "${project_root}" submodule status upstream/advance-firmware | grep -E '^[+-U]' || true)" ]] \
   || [[ -n "$(git -C "${project_root}/upstream/advance-firmware" status --porcelain)" ]]; then
  echo "upstream submodule is modified or not at the pinned commit" >&2
  exit 1
fi

echo "macOS check passed. Firmware screenshot: ${build_dir}/first-firmware-view.png"
