---
id: s4-macos-bundle
title: Package a local macOS app bundle
status: ready
priority: medium
estimate: M
depends_on: [s4-audio-validation, s4-midi-validation]
write_scope: [packaging/macos, CMakeLists.txt, docs/adr/0014-macos-bundle.md]
owner: unassigned
board: planning/sprint-4-macos-audio-midi.kanban.md
board_card_id: s4-macos-bundle
attempts: 0
---

## Goal

Produce a local macOS application bundle that runs outside the build directory.

## Acceptance criteria

- [ ] The .app includes its required runtime libraries and license notices, launches from a copied location, and retains its data directory across relaunch. The packaging method and bundle contents are recorded in ADR 0014.

## Inputs

- Pinned Advance v3.2.1 interfaces, Sprint 3 boot findings, and prerequisite job results.

## Output

- macOS bundle recipe and ADR 0014.

## Result

Pending.
