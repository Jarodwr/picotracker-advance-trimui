# Notices

PicoTracker for desktop and Trimui (this repository, the "wrapper") is licensed
under the GNU General Public License, version 3 only (`GPL-3.0-only`). The full
text is in [LICENSE](LICENSE) and [LICENSES/GPL-3.0-only.txt](LICENSES/GPL-3.0-only.txt).
Copyright (c) 2026 Jarod Wright and contributors.

Source: <https://github.com/Jarodwr/picotracker-advance-trimui>. The wrapper
does not copy the firmware sources; it builds the pinned
[xiphonics/advance-firmware](https://github.com/xiphonics/advance-firmware)
checkout in `upstream/advance-firmware` (a git submodule; `git ls-tree HEAD
upstream/advance-firmware` shows the exact commit). A built binary is a combined
work of the wrapper and that firmware and is distributed under `GPL-3.0-only`,
with the notices below preserved.

## File licences

Every tracked file carries an `SPDX-License-Identifier`, either in the file or
through [REUSE.toml](REUSE.toml). Check it with `reuse lint`.

- Original wrapper files: `GPL-3.0-only`.
- Files that mirror upstream declarations (`platform/desktop/include/**`, the
  fakes in `tests/macos_midi_fake/` and `tests/coremidi_fake/`) and
  `patches/0001-constexpr-literal-lengths.patch`: `BSD-3-Clause AND GPL-3.0-only`.
  Upstream's BSD notice and copyright lines are kept; the BSD text is in
  [LICENSES/BSD-3-Clause.txt](LICENSES/BSD-3-Clause.txt). Both licences apply.
- `packaging/trimui/licenses/*.txt` are verbatim third-party licence texts and
  keep their own terms (`LicenseRef-Third-Party-Licence-Text`).

## Third-party components

The Trimui pak lists every component it contains or links, with the licence text
for each, in `PicoTracker.pak/licenses/NOTICE.md`. The source table is
[packaging/trimui/licenses/components.tsv](packaging/trimui/licenses/components.tsv).
The macOS bundle's list is [packaging/macos/THIRD_PARTY_NOTICES.md](packaging/macos/THIRD_PARTY_NOTICES.md).
In short: upstream picoTracker (BSD-3-Clause baseline, GPL-3.0-only additions),
the Advance Ubuntu Mono font (Ubuntu Font Licence 1.0), Braids (MIT), ETL (MIT),
yxml (MIT), TinyXML2 (zlib), libsamplerate (BSD-2-Clause), cRSID-derived
AdvanceSID, Dattorro reverb (public domain), SDL2 (zlib), and libstdc++/libgcc
under the GCC Runtime Library Exception. Vendor libraries on the device are not
redistributed.
