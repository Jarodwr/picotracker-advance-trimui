#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Builds the Trimui Smart Pro release zip (ADR 0017 sections 2, 3, 6, 7) from an
# existing cross-built binary. The zip's only top-level directory is
# Tools/tg5040/PicoTracker.pak/; extract it at the SD card root.
#
# usage: package-trimui.sh [--binary PATH] [--out FILE] [--version V] [--upstream DIR] [--stage-only DIR]
#   --binary PATH     default: build/trimui/picotracker_desktop (output of scripts/build-trimui.sh).
#                     It is stripped in the package; the original is not touched.
#   --out FILE        default: build/trimui/PicoTracker-trimui-tg5040-<version>.zip
#   --version V       default: git describe --tags --always
#   --upstream DIR    firmware checkout holding the licence files (default: upstream/advance-firmware).
#   --stage-only DIR  write the pak tree under DIR and stop (no zip); used by tests.
# Environment: STRIP selects the strip tool (default: first of aarch64-linux-gnu-strip,
#   llvm-strip, strip found). SOURCE_DATE_EPOCH sets every timestamp (default: last commit time),
#   so two runs over the same inputs give the same SHA-256.
# Fails if licenses/NOTICE.md would list a file that is missing from the pak.
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
pkg_src="${project_root}/packaging/trimui"
binary="${project_root}/build/trimui/picotracker_desktop"
upstream="${project_root}/upstream/advance-firmware"
out=""
version=""
stage_only=""
pak_rel="Tools/tg5040/PicoTracker.pak"

die() { echo "package-trimui: $*" >&2; exit 1; }
say() { echo "[package-trimui] $*"; }

while [[ $# -gt 0 ]]; do
  case "$1" in
    --binary) [[ $# -ge 2 ]] || die "--binary needs a path"; binary="$2"; shift ;;
    --out) [[ $# -ge 2 ]] || die "--out needs a file"; out="$2"; shift ;;
    --version) [[ $# -ge 2 ]] || die "--version needs a value"; version="$2"; shift ;;
    --upstream) [[ $# -ge 2 ]] || die "--upstream needs a directory"; upstream="$2"; shift ;;
    --stage-only) [[ $# -ge 2 ]] || die "--stage-only needs a directory"; stage_only="$2"; shift ;;
    -h | --help) sed -n '2,17p' "${BASH_SOURCE[0]}"; exit 0 ;;
    *) die "unknown argument: $1" ;;
  esac
  shift
done

[[ -f "${binary}" ]] || die "binary not found: ${binary} (run scripts/build-trimui.sh)"
# A worktree has an empty submodule directory; take the licence files from the main checkout.
if [[ ! -d "${upstream}/LICENSES" && "${upstream}" == "${project_root}/upstream/advance-firmware" ]]; then
  common="$(git -C "${project_root}" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)"
  [[ -n "${common}" && -d "$(dirname "${common}")/upstream/advance-firmware/LICENSES" ]] && upstream="$(dirname "${common}")/upstream/advance-firmware"
fi
[[ -d "${upstream}/LICENSES" ]] || die "firmware checkout not found at ${upstream} (git submodule update --init)"
if [[ -z "${version}" ]]; then
  version="$(git -C "${project_root}" describe --tags --always 2>/dev/null || echo dev)"
fi
[[ -n "${out}" ]] || out="${project_root}/build/trimui/PicoTracker-trimui-tg5040-${version}.zip"
[[ "${out}" == /* ]] || out="${PWD}/${out}"
if [[ -z "${SOURCE_DATE_EPOCH:-}" ]]; then
  SOURCE_DATE_EPOCH="$(git -C "${project_root}" log -1 --format=%ct 2>/dev/null || echo 1700000000)"
fi

if [[ -n "${stage_only}" ]]; then
  stage="${stage_only}"
  mkdir -p "${stage}"
else
  stage="$(mktemp -d)"
  trap 'rm -rf "${stage}"' EXIT
fi
pak="${stage}/${pak_rel}"
rm -rf "${stage:?}/Tools"
mkdir -p "${pak}/bin" "${pak}/share" "${pak}/licenses"

# Binary: stripped copy.
cp "${binary}" "${pak}/bin/picotracker"
strip_tool="${STRIP:-}"
if [[ -z "${strip_tool}" ]]; then
  for candidate in aarch64-linux-gnu-strip llvm-strip strip; do
    if command -v "${candidate}" >/dev/null 2>&1; then strip_tool="${candidate}"; break; fi
  done
fi
if [[ -n "${strip_tool}" ]] && "${strip_tool}" "${pak}/bin/picotracker" 2>/dev/null; then
  say "stripped bin/picotracker with ${strip_tool}"
else
  say "warning: no strip tool worked; bin/picotracker is packaged as built"
fi

# Launcher, readme, shared data.
cp "${pkg_src}/launch.sh" "${pak}/launch.sh"
cp "${pkg_src}/README.txt" "${pak}/README.txt"
cp "${pkg_src}/share/input.ini.example" "${pak}/share/input.ini.example"
cp -R "${pkg_src}/share/default-data" "${pak}/share/default-data"

upstream_url="$(git -C "${project_root}" config -f "${project_root}/.gitmodules" submodule.upstream/advance-firmware.url 2>/dev/null || echo https://github.com/xiphonics/advance-firmware)"
upstream_url="${upstream_url%.git}"
upstream_rev="$(git -C "${project_root}" ls-tree HEAD upstream/advance-firmware 2>/dev/null | awk '{print $3}')"
upstream_rev="${upstream_rev:-unknown (see the submodule pin of upstream/advance-firmware)}"

# Licences from the component table; NOTICE.md is generated from the same rows.
notice="${pak}/licenses/NOTICE.md"
{
  echo "# Third-party notices"
  echo
  echo "PicoTracker for the Trimui Smart Pro bundles, statically links or is built against the components below. SDL2 itself is not shipped: the app loads the one in the device firmware (/usr/trimui/lib)."
  echo "Each licence text is in this folder."
  echo
  echo "PicoTracker itself (the wrapper) is licensed GPL-3.0-only: see wrapper-GPL-3.0-only.txt."
  echo "Source offer: the complete corresponding source of this build is at https://github.com/Jarodwr/picotracker-advance-trimui and, for the firmware it is built from, ${upstream_url} at commit ${upstream_rev}. If either is unavailable, write to the repository's issue tracker and the source will be provided."
  echo
  echo "| Component | Version | Licence | File |"
  echo "|---|---|---|---|"
} >"${notice}"
while IFS=$'\t' read -r name ver lic file src; do
  [[ -z "${name}" || "${name}" == \#* ]] && continue
  case "${src}" in
    upstream:*) from="${upstream}/${src#upstream:}" ;;
    template:*) from="${pkg_src}/licenses/${src#template:}" ;;
    repo:*) from="${project_root}/${src#repo:}" ;;
    *) die "bad source '${src}' for ${name}" ;;
  esac
  [[ -f "${from}" ]] || die "licence file for ${name} is missing: ${from}"
  cp "${from}" "${pak}/licenses/${file}"
  echo "| ${name} | ${ver} | ${lic} | \`${file}\` |" >>"${notice}"
done <"${pkg_src}/licenses/components.tsv"

bt='`'
# Consistency: every file NOTICE.md lists must exist in the pak.
listed=0
while IFS= read -r file; do
  [[ -f "${pak}/licenses/${file}" ]] || die "NOTICE.md lists ${file} but it is not in licenses/"
  listed=$((listed + 1))
done < <(sed -n "s/^| .* | ${bt}\\([^${bt}]*\\)${bt} |\$/\\1/p" "${notice}")
[[ ${listed} -gt 0 ]] || die "NOTICE.md lists no files"

# Modes and timestamps (zip stores both).
find "${stage}/Tools" -type d -exec chmod 0755 {} +
find "${stage}/Tools" -type f -exec chmod 0644 {} +
chmod 0755 "${pak}/launch.sh" "${pak}/bin/picotracker"
stamp="$(TZ=UTC date -r "${SOURCE_DATE_EPOCH}" +%Y%m%d%H%M.%S 2>/dev/null || TZ=UTC date -d "@${SOURCE_DATE_EPOCH}" +%Y%m%d%H%M.%S)"
find "${stage}/Tools" -exec touch -h -t "${stamp}" {} +

if [[ -n "${stage_only}" ]]; then
  say "staged ${pak} (${listed} licence files)"
  exit 0
fi

mkdir -p "$(dirname "${out}")"
rm -f "${out}"
(
  cd "${stage}"
  # Sorted names, no extra attributes (-X), no directory entries (-D): same bytes every run.
  find Tools -type f | LC_ALL=C sort | TZ=UTC zip -q -X -D -9 "${out}" -@
)
say "wrote ${out}"
if command -v shasum >/dev/null 2>&1; then shasum -a 256 "${out}"; else sha256sum "${out}"; fi
