#!/usr/bin/env python3
"""Small file-based scheduler for picoTracker jobs (Python standard library only)."""

import argparse
import datetime as dt
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent
STATUSES = {"draft", "ready", "queued", "running", "review", "blocked", "done", "cancelled"}
NEXT = {
    "draft": {"ready", "blocked", "cancelled"},
    "ready": {"queued", "blocked", "cancelled"},
    "queued": {"running", "ready", "blocked", "cancelled"},
    "running": {"review", "blocked", "cancelled"},
    "review": {"done", "ready", "blocked", "cancelled"},
    "blocked": {"ready", "cancelled"},
    "done": set(), "cancelled": set(),
}
REQUIRED = {"id", "title", "status", "priority", "depends_on", "write_scope", "owner", "attempts"}
DEFAULT_HEADINGS = ["To Do", "In Progress", "Blocked", "Review", "Done"]


def read_job(path):
    source = path.read_text()
    match = re.match(r"\A---\n(.*?)\n---\n", source, re.S)
    if not match:
        raise ValueError(f"{path}: missing frontmatter")
    fields = {}
    for line in match.group(1).splitlines():
        key, sep, value = line.partition(":")
        if not sep or not re.fullmatch(r"[a-z_]+", key):
            raise ValueError(f"{path}: unsupported frontmatter line: {line}")
        value = value.strip()
        if value.startswith("[") and value.endswith("]"):
            value = [v.strip().strip("\"'") for v in value[1:-1].split(",") if v.strip()]
        elif value.isdecimal():
            value = int(value)
        fields[key] = value
    return {"path": path, "fields": fields, "body": source[match.end():]}


def write_job(job):
    lines = []
    for key, value in job["fields"].items():
        if isinstance(value, list):
            value = "[" + ", ".join(value) + "]"
        lines.append(f"{key}: {value}")
    job["path"].write_text("---\n" + "\n".join(lines) + "\n---\n" + job["body"])


def load_jobs():
    jobs = {}
    for path in sorted((ROOT / "work").glob("pt-*.md")):
        job = read_job(path)
        ident = job["fields"].get("id")
        if ident in jobs:
            raise ValueError(f"duplicate job ID: {ident}")
        jobs[ident] = job
    return jobs


def load_config():
    config = json.loads((ROOT / "planning" / "orchestrator-config.json").read_text())
    if config.get("selection") != "priority_then_downstream_count":
        raise ValueError("unsupported selection policy")
    order = config.get("priority_order")
    if sorted(order) != sorted(["critical", "high", "medium", "low"]):
        raise ValueError("priority_order must contain each priority once")
    if not isinstance(config.get("max_parallel"), int) or config["max_parallel"] < 1:
        raise ValueError("max_parallel must be positive")
    return config


def validate(jobs, config):
    problems = []
    for ident, job in jobs.items():
        f = job["fields"]
        missing = REQUIRED - f.keys()
        if missing:
            problems.append(f"{ident}: missing {', '.join(sorted(missing))}")
        if f.get("status") not in STATUSES:
            problems.append(f"{ident}: invalid status")
        if f.get("priority") not in config["priority_order"]:
            problems.append(f"{ident}: invalid priority")
        if not isinstance(f.get("depends_on"), list) or not isinstance(f.get("write_scope"), list):
            problems.append(f"{ident}: depends_on and write_scope must be lists")
        if "## Goal" not in job["body"] or "## Acceptance criteria" not in job["body"] or "## Output" not in job["body"]:
            problems.append(f"{ident}: missing goal, criteria, or output section")
        for dep in f.get("depends_on", []):
            if dep not in jobs:
                problems.append(f"{ident}: unknown dependency {dep}")
    visiting, visited = set(), set()
    def visit(ident):
        if ident in visiting:
            problems.append(f"dependency cycle at {ident}")
            return
        if ident in visited:
            return
        visiting.add(ident)
        for dep in jobs[ident]["fields"].get("depends_on", []):
            if dep in jobs:
                visit(dep)
        visiting.remove(ident)
        visited.add(ident)
    for ident in jobs:
        visit(ident)
    return problems


def overlap(left, right):
    for a in left:
        for b in right:
            a, b = a.rstrip("/"), b.rstrip("/")
            if a == b or a.startswith(b + "/") or b.startswith(a + "/"):
                return True
    return False


def select(jobs, max_parallel, config):
    active = [j for j in jobs.values() if j["fields"]["status"] in {"queued", "running"}]
    slots = max(0, max_parallel - len(active))
    downstream = {ident: 0 for ident in jobs}
    for job in jobs.values():
        if job["fields"]["status"] != "done":
            for dep in job["fields"]["depends_on"]:
                downstream[dep] += 1
    candidates = [j for j in jobs.values() if j["fields"]["status"] == "ready" and
                  all(jobs[d]["fields"]["status"] == "done" for d in j["fields"]["depends_on"])]
    priority = {name: index for index, name in enumerate(config["priority_order"])}
    candidates.sort(key=lambda j: (priority[j["fields"]["priority"]], -downstream[j["fields"]["id"]], j["fields"]["id"]))
    chosen = []
    for job in candidates:
        scope = job["fields"]["write_scope"]
        if not scope or any(overlap(scope, other["fields"]["write_scope"]) for other in active + chosen):
            continue
        chosen.append(job)
        if len(chosen) == slots:
            break
    return chosen


def board_section(status):
    return {"draft": "To Do", "ready": "To Do", "queued": "To Do", "running": "In Progress",
            "review": "Review", "blocked": "Blocked", "done": "Done", "cancelled": "Done"}[status]


def update_board(job, status):
    f = job["fields"]
    if "board" not in f or "board_card_id" not in f:
        return
    path = ROOT / f["board"]
    original = path.read_text()
    pieces = re.split(r"(?=^## )", original, flags=re.M)
    headings = [re.match(r"## ([^\n]+)", p).group(1) for p in pieces[1:]]
    if len(headings) != len(set(headings)) or board_section(status) not in headings:
        raise ValueError(f"{path}: invalid or missing board headings")
    # The schema belongs to the existing board. A card move may not change it.
    schema = json.loads((ROOT / "planning" / "board-headings.json").read_text()).get(f["board"])
    if headings != schema:
        raise ValueError(f"{path}: board headings differ from saved schema")
    marker = f"<!-- id: {f['board_card_id']} -->"
    card_pattern = re.compile(r"^#### [^\n]+\n(?:(?!^#### |^## ).)*?" + re.escape(marker) + r"(?:(?!^#### |^## ).)*", re.M | re.S)
    found = [(i, m) for i, part in enumerate(pieces[1:], 1) for m in card_pattern.finditer(part)]
    if len(found) != 1:
        raise ValueError(f"{path}: expected exactly one card {f['board_card_id']}")
    index, match = found[0]
    card = match.group().strip()
    pieces[index] = pieces[index][:match.start()] + pieces[index][match.end():]
    target = headings.index(board_section(status)) + 1
    pieces[target] = pieces[target].rstrip() + "\n\n" + card + "\n\n"
    result = "".join(pieces).rstrip() + "\n"
    new_headings = re.findall(r"^## ([^\n]+)", result, re.M)
    if new_headings != headings:
        raise ValueError("board headings changed; refusing write")
    path.write_text(result)


def transition(jobs, ident, target, reason, max_parallel, config):
    job = jobs[ident]
    current = job["fields"]["status"]
    if target not in NEXT[current]:
        raise ValueError(f"invalid transition {current} -> {target}")
    if target in {"ready", "queued"} and any(jobs[d]["fields"]["status"] != "done" for d in job["fields"]["depends_on"]):
        raise ValueError("required dependencies are unfinished")
    if target == "queued" and job not in select(jobs, max_parallel, config):
        raise ValueError("job is not selectable under current parallel limit")
    update_board(job, target)
    job["fields"]["status"] = target
    if target == "running":
        job["fields"]["attempts"] += 1
    write_job(job)
    log = ROOT / "runs" / "history.md"
    log.parent.mkdir(exist_ok=True)
    with log.open("a") as out:
        out.write(f"- {dt.datetime.now(dt.timezone.utc).isoformat()} `{ident}`: {current} → {target}. {reason}\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("validate")
    plan = sub.add_parser("plan")
    plan.add_argument("--max-parallel", type=int)
    move = sub.add_parser("transition")
    move.add_argument("id")
    move.add_argument("status", choices=sorted(STATUSES))
    move.add_argument("--reason", required=True)
    move.add_argument("--max-parallel", type=int)
    brief = sub.add_parser("brief")
    brief.add_argument("id")
    args = parser.parse_args()
    config = load_config()
    jobs = load_jobs()
    problems = validate(jobs, config)
    if problems:
        parser.exit(1, "\n".join(problems) + "\n")
    if args.command == "validate":
        print(f"Valid: {len(jobs)} jobs, no dependency cycles")
    elif args.command == "plan":
        limit = args.max_parallel or config["max_parallel"]
        if limit < 1:
            parser.error("max-parallel must be positive")
        for job in select(jobs, limit, config):
            print(f"{job['fields']['id']}: {job['fields']['title']}")
    elif args.command == "brief":
        if args.id not in jobs:
            parser.error(f"unknown job: {args.id}")
        job = jobs[args.id]
        f = job["fields"]
        print(f"Job: {f['id']} — {f['title']}\nStatus: {f['status']}\nDependencies: {', '.join(f['depends_on']) or 'none'}\nWrite scope: {', '.join(f['write_scope']) or 'read-only'}\n")
        print(job["body"].strip())
        print("\nReturn outcome, acceptance-criteria evidence, changed files, blockers, and follow-up suggestions. Do not edit board headings or other job records.")
    else:
        if args.id not in jobs:
            parser.error(f"unknown job: {args.id}")
        limit = args.max_parallel or config["max_parallel"]
        if limit < 1:
            parser.error("max-parallel must be positive")
        transition(jobs, args.id, args.status, args.reason, limit, config)
        print(f"{args.id}: {args.status}")


if __name__ == "__main__":
    main()
