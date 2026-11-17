---
id: pt-prototype-plan
title: Select a desktop prototype approach
status: done
priority: high
estimate: M
depends_on: [pt-platform-map, pt-license-review, pt-memory-limits]
write_scope: [planning/findings/prototype-plan.md]
owner: unassigned
board: planning/sprint-1-investigation.kanban.md
board_card_id: s1-prototype-plan
attempts: 1
---

## Goal

Choose the smallest desktop proof of concept supported by the Advance source investigation.

## Acceptance criteria

- [ ] Recommend a host framework, source selection, build strategy, and first runnable milestone.
- [ ] Explain known feature gaps, patch needs, and licensing constraints.
- [ ] State what can run on macOS first and what must change for Windows and Linux.

## Inputs

- `pt-platform-map`, `pt-license-review`, and `pt-memory-limits` outputs.

## Output

- `planning/findings/prototype-plan.md`

## Result

Completed from all three prerequisite findings. The plan chooses a wrapper-owned CMake/SDL host target and an initial UI/input milestone, with later audio and fixture checks. It records current Advance feature parity as unavailable from the pinned public source. Changed file: `planning/findings/prototype-plan.md`.
