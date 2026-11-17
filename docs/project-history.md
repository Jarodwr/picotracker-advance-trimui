# Project history and planning material

You do not need any of this to build or use the port. It records how the project was run early on.

- [planning/](../planning/): Kanban boards (`*.kanban.md`, written for the VS Code MD Kanban extension), the [roadmap](../planning/roadmap.md), the [orchestrator design](../planning/orchestrator-design.md) and the measurements and evidence in [planning/findings/](../planning/findings/), which the README and the ADRs cite.
- [work/](../work/): one Markdown job record per board card (see its [README](../work/README.md)).
- [runs/history.md](../runs/history.md): the scheduler's run log.
- [orchestrator.py](../orchestrator.py) and [ORCHESTRATOR.md](../ORCHESTRATOR.md): a small local scheduler that validates job records, selects ready work and moves cards. `python3 orchestrator.py validate` and `python3 orchestrator.py plan` inspect the queue.

These files were left in place rather than moved because the ADRs, the findings and the job records link to each other by path.
