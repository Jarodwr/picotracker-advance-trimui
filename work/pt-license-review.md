---
id: pt-license-review
title: Review licenses and dependencies
status: done
priority: high
estimate: S
depends_on: [pt-source-baseline]
write_scope: [planning/findings/license-review.md]
owner: unassigned
board: planning/sprint-1-investigation.kanban.md
board_card_id: s1-licenses
attempts: 1
---

## Goal
Inventory licenses and redistribution obligations for the selected upstream source and dependencies relevant to a desktop build.

## Acceptance criteria
- Identify license files and dependency declarations at the selected revision.
- Note attribution, notices and redistribution requirements for source and assets.
- Flag unclear or missing license information for human review.

## Inputs
- `pt-source-baseline` output.

## Output
- `planning/findings/license-review.md`

## Result

Completed at pinned `v2.3.1`. The finding inventories the root license, referenced third-party licenses, font terms, nested submodules and notice-bundle requirements. Changed file: `planning/findings/license-review.md`.
