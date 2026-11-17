---
id: pt-platform-map
title: Map tracker and platform boundaries
status: done
priority: high
estimate: M
depends_on: [pt-source-baseline]
write_scope: [planning/findings/platform-map.md]
owner: unassigned
board: planning/sprint-1-investigation.kanban.md
board_card_id: s1-platform-map
attempts: 1
---

## Goal
Identify reusable tracker, UI and audio code and the hardware-facing interfaces that need desktop implementations.

## Acceptance criteria
- Cite entry points and relevant source paths at the pinned revision.
- Cover display, input, audio, storage, timing and MIDI dependencies.
- Propose likely adapter boundaries and list unresolved questions.

## Inputs
- `pt-source-baseline` output.

## Output
- `planning/findings/platform-map.md`

## Result

Completed at pinned `v2.3.1`. The map identifies shared CMake targets, boot/service installation, seven host adapter concerns and a first UI/input slice. Changed file: `planning/findings/platform-map.md`. Compile feasibility remains a sprint 2 prototype question.
