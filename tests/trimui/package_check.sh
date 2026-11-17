#!/usr/bin/env bash

# Copyright (c) 2026 Jarod Wright and contributors
#
# SPDX-License-Identifier: GPL-3.0-only

# Checks scripts/package-trimui.sh: zip tree, modes, shell syntax and shellcheck,
# NOTICE.md consistency, the missing-licence failure, and byte-identical zips from two runs.
# Uses a fake binary, so it needs no cross build. Requires zip, unzip and shellcheck.
# shellcheck disable=SC2015 # A && ok || fail is intentional
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
pkg="${project_root}/scripts/package-trimui.sh"
pak="Tools/tg5040/PicoTracker.pak"
work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT
failures=0

fail() { echo "FAIL: $*" >&2; failures=$((failures + 1)); }
ok() { echo "ok: $*"; }
sha() { if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1; else sha256sum "$1" | cut -d' ' -f1; fi; }

command -v shellcheck >/dev/null 2>&1 || { echo "FAIL: shellcheck is required" >&2; exit 1; }

head -c 4096 /dev/urandom >"${work}/fake-binary"
# Different wall-clock times must not change the bytes: SOURCE_DATE_EPOCH is fixed.
export SOURCE_DATE_EPOCH=1700000000

"${pkg}" --binary "${work}/fake-binary" --version test --out "${work}/a.zip" >"${work}/a.log" 2>&1 || { cat "${work}/a.log"; fail "package run 1"; }
sleep 1
"${pkg}" --binary "${work}/fake-binary" --version test --out "${work}/b.zip" >"${work}/b.log" 2>&1 || { cat "${work}/b.log"; fail "package run 2"; }
[[ -f "${work}/a.zip" && -f "${work}/b.zip" ]] || { echo "no zip produced" >&2; exit 1; }

if [[ "$(sha "${work}/a.zip")" == "$(sha "${work}/b.zip")" ]]; then ok "two runs give the same SHA-256"; else fail "zips differ between runs"; fi

# Tree: one top-level directory, the expected files.
tops="$(unzip -Z1 "${work}/a.zip" | cut -d/ -f1 | sort -u)"
if [[ "${tops}" == "Tools" ]]; then ok "only top-level directory is Tools/"; else fail "top level: ${tops}"; fi
if unzip -Z1 "${work}/a.zip" | grep -v "^${pak}/" | grep -q .; then fail "entries outside ${pak}/"; else ok "all entries under ${pak}/"; fi
for f in launch.sh bin/picotracker README.txt share/input.ini.example share/default-data/projects/.keep \
  share/default-data/samples/.keep licenses/NOTICE.md; do
  unzip -Z1 "${work}/a.zip" | grep -qx "${pak}/${f}" && ok "has ${f}" || fail "missing ${f}"
done
if unzip -Z1 "${work}/a.zip" | grep -q "^${pak}/lib/"; then fail "lib/ must be absent (the vendor SDL2 is not bundled)"; else ok "no lib/"; fi

# Modes.
mode_of() { unzip -Z "${work}/a.zip" "${pak}/$1" | awk '{print $1}'; }
[[ "$(mode_of launch.sh)" == "-rwxr-xr-x" ]] && ok "launch.sh is 0755" || fail "launch.sh mode $(mode_of launch.sh)"
[[ "$(mode_of bin/picotracker)" == "-rwxr-xr-x" ]] && ok "bin/picotracker is 0755" || fail "picotracker mode"
[[ "$(mode_of README.txt)" == "-rw-r--r--" ]] && ok "README.txt is 0644" || fail "README mode"

# Extract and check the scripts.
mkdir "${work}/x"
unzip -q "${work}/a.zip" -d "${work}/x"
launch="${work}/x/${pak}/launch.sh"
if sh -n "${launch}"; then ok "launch.sh passes sh -n"; else fail "sh -n launch.sh"; fi
if shellcheck -s sh "${launch}"; then ok "launch.sh is shellcheck clean"; else fail "shellcheck launch.sh"; fi
if shellcheck -x "${pkg}" "${project_root}/scripts/deploy-trimui.sh" "${BASH_SOURCE[0]}"; then ok "scripts are shellcheck clean"; else fail "shellcheck scripts"; fi
if grep -q -- '--display trimui' "${launch}" && grep -q -- '--data-dir' "${launch}"; then ok "launch.sh passes --display trimui and --data-dir"; else fail "launch.sh arguments"; fi
if grep -q -- '--log-input' "${launch}" | grep -v '^#' >/dev/null 2>&1 && grep -v '^#' "${launch}" | grep -q -- '--log-input'; then fail "launch.sh passes unimplemented --log-input"; else ok "launch.sh does not pass --log-input"; fi

if grep -q 'LD_LIBRARY_PATH' "${launch}" && grep -q '/usr/trimui/lib' "${launch}"; then ok "launch.sh puts /usr/trimui/lib on LD_LIBRARY_PATH"; else fail "launch.sh lacks vendor LD_LIBRARY_PATH"; fi
if grep -v '^[[:space:]]*#' "${launch}" | grep -Eq 'SDL_VIDEODRIVER|SDL_RENDER_DRIVER'; then fail "launch.sh forces an SDL driver"; else ok "launch.sh sets no SDL driver"; fi
if grep -q 'mixer_save' "${launch}" && grep -q 'mixer_restore' "${launch}"; then ok "launch.sh keeps the mixer save and restore"; else fail "mixer save/restore missing"; fi
if unzip -Z1 "${work}/a.zip" | grep -Eqi 'libSDL2|libEGL|libGLES|\.so'; then fail "pak bundles a shared library"; else ok "pak bundles no vendor library"; fi

bt='`'
# NOTICE.md consistency.
notice="${work}/x/${pak}/licenses/NOTICE.md"
listed=0
while IFS= read -r f; do
  listed=$((listed + 1))
  [[ -s "${work}/x/${pak}/licenses/${f}" ]] || fail "NOTICE.md lists ${f}, missing or empty"
done < <(sed -n "s/^| .* | ${bt}\\([^${bt}]*\\)${bt} |\$/\\1/p" "${notice}")
[[ ${listed} -ge 10 ]] && ok "NOTICE.md lists ${listed} files, all present" || fail "NOTICE.md lists only ${listed}"
extra="$(cd "${work}/x/${pak}/licenses" && for f in *; do [[ "${f}" == NOTICE.md ]] || grep -q "\`${f}\`" NOTICE.md || echo "${f}"; done)"
[[ -z "${extra}" ]] && ok "every licence file is listed in NOTICE.md" || fail "unlisted licence files: ${extra}"

# The pak carries the wrapper's own GPL text and a source offer naming the repository and firmware.
cmp -s "${work}/x/${pak}/licenses/wrapper-GPL-3.0-only.txt" "${project_root}/LICENSES/GPL-3.0-only.txt" \
  && ok "pak carries the wrapper GPL-3.0 text" || fail "wrapper GPL-3.0 text missing from the pak"
grep -q "github.com/Jarodwr/picotracker-advance-trimui" "${notice}" && grep -q "github.com/xiphonics/advance-firmware at commit [0-9a-f]\{40\}" "${notice}" \
  && ok "NOTICE.md has the source offer with the exact firmware commit" || fail "source offer incomplete in NOTICE.md"

# Packaging fails when a listed licence is missing.
if "${pkg}" --binary "${work}/fake-binary" --upstream "${work}/no-such-upstream" --out "${work}/c.zip" >"${work}/c.log" 2>&1; then
  fail "packaging succeeded with a missing firmware checkout"
else ok "packaging fails without the firmware licences"; fi
mkdir -p "${work}/up/LICENSES"
if "${pkg}" --binary "${work}/fake-binary" --upstream "${work}/up" --out "${work}/d.zip" >"${work}/d.log" 2>&1; then
  fail "packaging succeeded with a missing licence file"
elif grep -q "missing" "${work}/d.log"; then ok "packaging fails naming a missing licence file"; else fail "wrong failure message: $(cat "${work}/d.log")"; fi

# deploy --dry-run shows the new layout and changes nothing.
dry="$("${project_root}/scripts/deploy-trimui.sh" --dry-run --sd-root "${work}/sd" 2>&1)" || fail "deploy --dry-run"
echo "${dry}" | grep -q "PicoTracker.pak/licenses/NOTICE.md" && echo "${dry}" | grep -q "PicoTrackerSmoke.pak/launch.sh" \
  && ok "deploy --dry-run prints the packaged layout" || fail "deploy --dry-run layout"
[[ ! -e "${work}/sd" ]] && ok "dry run created nothing" || fail "dry run wrote to the SD root"

if [[ ${failures} -ne 0 ]]; then echo "package_check: ${failures} failure(s)" >&2; exit 1; fi
echo "package_check: PASS"
