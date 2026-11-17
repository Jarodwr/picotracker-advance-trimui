---
id: pt-source-baseline
title: Identify the public Advance source baseline
status: done
priority: critical
estimate: M
depends_on: []
write_scope: [planning/findings/source-baseline.md]
owner: unassigned
board: planning/sprint-1-investigation.kanban.md
board_card_id: s1-source-baseline
attempts: 2
---

## Goal
Identify the public picoTracker revision that best represents the Advance firmware target and establish what code is available there.

## Acceptance criteria
- Record repository URL, exact commit/tag, and retrieval date.
- Compare the source with the current documented Advance features and note verified gaps.
- Cite source files, tags/releases, and documentation for factual claims.
- Make no unsupported claim of feature parity.

## Inputs
- Project objective: desktop/multiplatform Advance experience built around upstream as a submodule.

## Output
- `planning/findings/source-baseline.md`

## Result

Completed from the official upstream clone on 2026-11-15. Pinned the submodule to `v2.3.1` (`ce77d1f5b31f75a9b772384572ed6705e8c86e3d`). The output identifies the later repository split and the gap between published v2.3.1 source and current Advance instrument documentation. Changed files: `.gitmodules`, submodule gitlink, and `planning/findings/source-baseline.md`. The next jobs can inspect the pinned source. Current Advance feature parity remains an open product decision.
