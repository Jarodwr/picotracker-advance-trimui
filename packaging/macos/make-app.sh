#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Assemble picoTracker Advance Desktop.app (docs/adr/0015-macos-bundle.md).
# usage: make-app.sh EXECUTABLE OUTPUT_DIR INFO_PLIST UPSTREAM_ROOT PROJECT_ROOT
set -euo pipefail

exe="$1"; out="$2"; plist="$3"; upstream="$4"; root="$5"
app="${out}/picoTracker Advance Desktop.app"
contents="${app}/Contents"
frameworks="${contents}/Frameworks"
licenses="${contents}/Resources/licenses"

rm -rf "${app}"
mkdir -p "${contents}/MacOS" "${frameworks}" "${licenses}"
cp "${exe}" "${contents}/MacOS/picotracker_desktop"
cp "${plist}" "${contents}/Info.plist"
bin="${contents}/MacOS/picotracker_desktop"

# Third-party dylib closure: anything not in /usr/lib or /System.
resolve() { # dependency path as recorded in $2, owner file $2
  case "$1" in
    @rpath/*) local name="${1#@rpath/}"
      while read -r rp; do
        [[ -e "${rp}/${name}" ]] && { echo "${rp}/${name}"; return; }
      done < <(otool -l "$2" | awk '/LC_RPATH/{f=1} f&&/path /{print $2; f=0}')
      for d in /opt/homebrew/lib /usr/local/lib; do
        [[ -e "${d}/${name}" ]] && { echo "${d}/${name}"; return; }
      done ;;
    /*) echo "$1" ;;
  esac
}
declare -a queue=("${bin}")
seen=" "
while ((${#queue[@]})); do
  cur="${queue[0]}"; queue=("${queue[@]:1}")
  while read -r dep; do
    case "${dep}" in /usr/lib/*|/System/*) continue ;; esac
    real="$(resolve "${dep}" "${cur}")"
    [[ -n "${real}" && -e "${real}" ]] || { echo "cannot resolve ${dep}" >&2; exit 1; }
    real="$(cd "$(dirname "${real}")" && pwd -P)/$(basename "${real}")"
    base="$(basename "${real}")"
    if [[ "${seen}" != *" ${base} "* ]]; then
      seen+="${base} "
      cp "${real}" "${frameworks}/${base}"; chmod u+w "${frameworks}/${base}"
      install_name_tool -id "@rpath/${base}" "${frameworks}/${base}"
      queue+=("${frameworks}/${base}")
    fi
    install_name_tool -change "${dep}" "@rpath/${base}" "${cur}"
  done < <(otool -L "${cur}" | tail -n +2 | awk '{print $1}')
done
# sdl2-compat dlopen()s libSDL3.dylib from its own directory at runtime, so it
# is invisible to otool. Bundle it under that exact name.
if strings "${frameworks}"/libSDL2-2.0.0.dylib | grep -q '@loader_path/libSDL3.dylib'; then
  sdl3=""
  for d in /opt/homebrew/lib /usr/local/lib; do
    [[ -e "${d}/libSDL3.dylib" ]] && { sdl3="${d}/libSDL3.dylib"; break; }
  done
  [[ -n "${sdl3}" ]] || { echo "libSDL3.dylib not found" >&2; exit 1; }
  cp "${sdl3}" "${frameworks}/libSDL3.dylib"; chmod u+w "${frameworks}/libSDL3.dylib"
  install_name_tool -id "@rpath/libSDL3.dylib" "${frameworks}/libSDL3.dylib"
fi
# Drop build-tree/Homebrew rpaths so only the bundle can satisfy @rpath.
while read -r rp; do install_name_tool -delete_rpath "${rp}" "${bin}"; done \
  < <(otool -l "${bin}" | awk '/LC_RPATH/{f=1} f&&/path /{print $2; f=0}')
install_name_tool -add_rpath "@executable_path/../Frameworks" "${bin}"
for f in "${frameworks}"/*.dylib; do
  [[ -e "$f" ]] && install_name_tool -add_rpath "@loader_path" "$f" 2>/dev/null || true
done

# License notices.
cp "${root}/packaging/macos/THIRD_PARTY_NOTICES.md" "${licenses}/"
cp "${upstream}/LICENSING.md" "${licenses}/"
cp -R "${upstream}/LICENSES" "${licenses}/upstream-LICENSES"
ext="${upstream}/sources/Externals"
cp "${ext}/braids/LICENSE" "${licenses}/braids-LICENSE"
cp "${ext}/etl/LICENSE" "${licenses}/etl-LICENSE"
cp "${ext}/yxml/COPYING" "${licenses}/yxml-COPYING"
cp "${ext}/TinyXML2/LICENSE.txt" "${licenses}/tinyxml2-LICENSE.txt"
for prefix in /opt/homebrew/opt /usr/local/opt; do
  [[ -e "${prefix}/sdl2-compat/share/licenses/sdl2-compat/LICENSE.txt" ]] &&
    cp "${prefix}/sdl2-compat/share/licenses/sdl2-compat/LICENSE.txt" "${licenses}/SDL2-compat-LICENSE.txt"
  [[ -e "${prefix}/sdl3/LICENSE.txt" ]] && cp "${prefix}/sdl3/LICENSE.txt" "${licenses}/SDL3-LICENSE.txt"
  [[ -e "${prefix}/sdl2/LICENSE.txt" ]] && cp "${prefix}/sdl2/LICENSE.txt" "${licenses}/SDL2-LICENSE.txt"
done

# Ad-hoc sign (required for arm64; install_name_tool invalidates signatures).
codesign --force --sign - "${frameworks}"/*.dylib
codesign --force --sign - "${app}"
codesign --verify --strict "${app}"
echo "Built ${app}"
