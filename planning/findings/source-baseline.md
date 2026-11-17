# Public Advance source baseline

## Update: dedicated Advance repository adopted 2026-11-15

Xiphonics now publishes [advance-firmware](https://github.com/xiphonics/advance-firmware), described as stable source snapshots of picoTracker Advance. This project's submodule now pins `v3.2.1` at `0282da7e50cef0bdc88e4084ae667dfd6ff55f1a`. It includes the FM6, StarLoom and PicoSwarm implementations absent from the earlier `v2.3.1` source. The dedicated repository's `LICENSING.md` states that new original contributions after its BSD baseline are `GPL-3.0-only`, with mixed-license files and inherited third-party material. See the [migration finding](source-migration.md) for build evidence and remaining work.

The sections below record the initial investigation of the older combined repository and its original pin. Their recommendation and conclusion that newer Advance source was unavailable have been superseded.

Checked 2026-11-15 (Australia/Melbourne) against a local clone of the official repository.

## Historical initial pin

The initial [xiphonics/picoTracker](https://github.com/xiphonics/picoTracker) submodule was pinned to tag `v2.3.1`, commit `ce77d1f5b31f75a9b772384572ed6705e8c86e3d` (2026-03-25). It was the latest release tag in that combined repository containing `sources/Adapters/adv`; `v2.4beta1` and `v2.4.0` lack that adapter.

The Advance adapter has display, audio, filesystem, MIDI, platform and sample-pool implementations. Shared code is under `sources/Application`, `sources/UIFramework`, `sources/Services` and `sources/System`. The Advance CMake configuration targets STM32H7 hardware.

Upstream commit `e6d8756c` removed the Advance adapter in a repository split. Its parent, `173dd3ea38a94378ef86ee43f3ae16db4bf1193b` (2026-04-07), is a later **public, untagged** snapshot containing the adapter. It is on a different branch of history from `v2.3.1`, so it is not simply an upgrade of that release. The `2.3-release` branch has later manual changes but no source changes under `sources/Adapters/adv` or `sources/Application` since `v2.3.1`. The tagged release remains the initial reproducible pin; compare the untagged snapshot for useful fixes before considering a repin.

## Feature comparison

The current [Advance instrument manual](https://manual.xiphonics.com/advance/instruments.html) describes FM6, StarLoom and PicoSwarm. Searches of the pinned `sources/Application` and `sources/Adapters/adv` for those names found no implementation. The pinned public source is therefore not feature equivalent to the current documented Advance firmware. The dedicated `advance-firmware` repository supplies source for those later features.

The [Advance introduction](https://manual.xiphonics.com/advance/introduction.html) describes 48 MB of sample memory per project and a 720×720 display. The exact limits and APIs in the pinned source belong in the platform and memory investigations.

## Superseded next decision

The source migration is recorded in `source-migration.md`. Future implementation should use the dedicated source and its license inventory.

## Evidence

The local clone was inspected with `git rev-parse HEAD`, `git ls-tree` for Advance adapter presence across tags, `git show --stat e6d8756c`, and `rg -i 'FM6|StarLoom|PicoSwarm'` against application and Advance adapter sources.
