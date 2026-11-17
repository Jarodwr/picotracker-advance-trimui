# Contributing

Thanks for looking. This is a small wrapper around Xiphonics' unmodified Advance firmware, so a few rules keep it that way.

- **Do not edit `upstream/advance-firmware`.** Firmware bugs belong to [xiphonics/advance-firmware](https://github.com/xiphonics/advance-firmware). A fix we want to carry goes in `patches/` as a patch, with an ADR if it changes the boundary ([ADR 0001](docs/adr/0001-upstream-boundary.md)).
- **Build and test before a pull request.** macOS: `bash scripts/check-macos.sh`. Linux: `scripts/check-linux.sh`. Trimui: `scripts/build-trimui.sh`, `tests/trimui/package_check.sh` and `tests/trimui/emulated_smoke.sh` (need Docker). Say what you ran on a real device and what you did not.
- **Measure, then claim.** Status statements in the README should point to evidence in `planning/findings/`.
- **Decisions** that change architecture get a short ADR in `docs/adr/`.
- **Licence headers.** New files carry `SPDX-License-Identifier: GPL-3.0-only` (see [NOTICE.md](NOTICE.md)); contributions are accepted under that licence.
- **Never commit device credentials.** The device scripts read `TRIMUI_SSH_*` from the environment.
