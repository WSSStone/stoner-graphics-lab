#!/usr/bin/env python3
"""Durable command-line controller for whole-project code reviews."""

from __future__ import annotations

import argparse
import importlib.metadata
import json
import os
import platform
import re
import shutil
import sys
from pathlib import Path
from typing import Any

TOOLS_DIR = Path(__file__).resolve().parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

import architecture_scan
import codegraph_adapter
import gate_runner
import spec_trace
from reviewlib import (
    FINDING_STATES,
    SCHEMA_VERSION,
    SEVERITIES,
    ReviewError,
    append_event,
    atomic_write,
    find_repo_root,
    git_snapshot,
    load_run,
    read_csv,
    read_json,
    run,
    utc_now,
    write_json,
)


def review_steps(*domains: str) -> list[str]:
    """Create bounded inspect/fix/verify packets for responsibility domains."""
    return [
        f"{action}: {domain}"
        for domain in domains
        for action in ("Inspect", "Fix", "Verify")
    ]


BATCHES = [
    ("B00", "Bootstrap", ["Framework and Draft PR", "Baseline and CodeGraph"]),
    ("B01", "Build, CI, and Architecture", ["Inspect", "Fix", "Verify"]),
    (
        "B02",
        "Core Features 003-006",
        review_steps(
            "value identity and containers",
            "memory allocation and module lifecycle",
            "scalar, vector, and color math",
            "matrix, quaternion, transform, and geometry math",
            "logging system",
            "assertion and platform-break macros",
            "platform selection, window, misc, and memory",
            "platform time, filesystem, and process",
        ),
    ),
    (
        "B03",
        "RHI Features 007-008",
        review_steps(
            "device, capabilities, runtime, and results",
            "commands, queues, synchronization, and swapchain",
            "buffer, texture, and sampler resources",
            "shaders, descriptors, pipelines, render passes, and framebuffers",
        ),
    ),
    (
        "B04",
        "Vulkan Foundation 009-010",
        review_steps(
            "instance, adapter, device, and capabilities",
            "surface and swapchain lifecycle",
            "memory allocation, buffers, and textures",
            "descriptors, samplers, and upload staging",
        ),
    ),
    (
        "B05",
        "Vulkan Execution 011-012",
        review_steps(
            "command pools, buffers, barriers, and render scope",
            "queues, submission, and synchronization",
            "shader modules and interfaces",
            "graphics and compute pipelines and cache",
            "native context execution",
        ),
    ),
    (
        "B06",
        "Renderer 013-015",
        review_steps(
            "render graph declaration, compilation, and lifetimes",
            "render graph execution, resources, and diagnostics",
            "shader library and permutations",
            "material definitions, instances, and parameters",
            "forward frame planning, lights, views, and sorting",
            "forward execution, graph declaration, and diagnostics",
        ),
    ),
    (
        "B07",
        "Application 016-017",
        review_steps(
            "window lifecycle, drivers, and loop",
            "input mapping, events, and snapshots",
            "entity slots and components",
            "hierarchy, reparenting, and destruction",
            "transforms and render collection",
        ),
    ),
    (
        "B08",
        "Integration 018-019",
        review_steps(
            "triangle demo configuration and lifecycle",
            "triangle native session, presentation, and synchronization",
            "deferred frame plan, surfaces, and graph",
            "deferred native execution and readback",
            "validation, failure injection, and artifacts",
        ),
    ),
    (
        "B09",
        "Cross-Cutting",
        review_steps(
            "test target architecture and private boundaries",
            "diagnostics, determinism, and failure semantics",
            "concurrency, lifetime, and ownership duplication",
            "performance hotspots and large functions",
            "documentation, specification drift, and traceability",
        ),
    ),
    ("B10", "Closeout", ["Traceability", "Final Gates", "Close"]),
]

FINDING_TRANSITIONS = {
    "Open": {"Triaged"},
    "Triaged": {"Accepted", "Deferred", "Rejected"},
    "Accepted": {"Fixed", "Deferred"},
    "Fixed": {"Verified"},
    "Deferred": set(),
    "Rejected": set(),
    "Verified": set(),
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--id", dest="global_id", help="review ID, for example CR-001")
    subparsers = parser.add_subparsers(dest="command", required=True)

    init_parser = subparsers.add_parser("init")
    init_parser.add_argument("--id", required=True)
    init_parser.add_argument("--slug", required=True)
    init_parser.add_argument("--baseline", required=True)
    init_parser.add_argument("--branch")

    for command in (
        "doctor",
        "baseline",
        "status",
        "next",
        "recover",
        "refine",
        "render",
        "lint",
        "close",
    ):
        child = subparsers.add_parser(command)
        child.add_argument("--id")

    start_parser = subparsers.add_parser("start")
    start_parser.add_argument("--id")

    complete_parser = subparsers.add_parser("complete")
    complete_parser.add_argument("--id")
    complete_parser.add_argument("--evidence", action="append", default=[])
    complete_parser.add_argument("--commit")
    complete_parser.add_argument("--note", default="")

    fail_parser = subparsers.add_parser("fail")
    fail_parser.add_argument("--id")
    fail_parser.add_argument("--reason", required=True)

    trace_parser = subparsers.add_parser("trace")
    trace_parser.add_argument("--id")

    gate_parser = subparsers.add_parser("gate")
    gate_parser.add_argument("--id")
    gate_parser.add_argument("profile", choices=gate_runner.available_profiles())

    finding_parser = subparsers.add_parser("finding")
    finding_parser.add_argument("--id")
    finding_subparsers = finding_parser.add_subparsers(dest="finding_command", required=True)

    add_parser = finding_subparsers.add_parser("add")
    add_parser.add_argument("--batch", required=True)
    add_parser.add_argument("--severity", required=True, choices=sorted(SEVERITIES))
    add_parser.add_argument("--title", required=True)
    add_parser.add_argument("--requirement", required=True)
    add_parser.add_argument("--location", required=True)
    add_parser.add_argument("--evidence", required=True)
    add_parser.add_argument("--impact", required=True)

    for action in ("triage", "fix", "verify"):
        action_parser = finding_subparsers.add_parser(action)
        action_parser.add_argument("finding_id")
        action_parser.add_argument("--note", required=True)
        if action == "triage":
            action_parser.add_argument(
                "--disposition", required=True, choices=("Accepted", "Rejected")
            )
        if action == "fix":
            action_parser.add_argument("--commit", required=True)

    defer_parser = finding_subparsers.add_parser("defer")
    defer_parser.add_argument("finding_id")
    defer_parser.add_argument("--target", required=True)
    defer_parser.add_argument("--reason", required=True)

    return parser.parse_args()


def command_run_id(args: argparse.Namespace) -> str | None:
    return getattr(args, "id", None) or args.global_id


def make_batches() -> list[dict[str, Any]]:
    batches = []
    for batch_id, title, step_titles in BATCHES:
        steps = [
            {
                "id": f"{batch_id}-S{index:02d}",
                "title": title_text,
                "status": "Pending",
                "started_at": None,
                "completed_at": None,
                "commit": None,
                "evidence": [],
                "note": "",
            }
            for index, title_text in enumerate(step_titles, start=1)
        ]
        batches.append(
            {"id": batch_id, "title": title, "status": "Pending", "steps": steps}
        )
    return batches


def refine_pending_batches(state: dict[str, Any]) -> list[str]:
    """Replace only pristine pending batches with the current packet definitions."""
    definitions = {batch["id"]: batch for batch in make_batches()}
    refined: list[str] = []
    for index, batch in enumerate(state["batches"]):
        replacement = definitions.get(batch["id"])
        if replacement is None:
            continue
        pristine = batch["status"] == "Pending" and all(
            step["status"] == "Pending" and step["started_at"] is None
            for step in batch["steps"]
        )
        if not pristine:
            continue
        if batch != replacement:
            state["batches"][index] = replacement
            refined.append(batch["id"])
    pending = next_pending(state)
    state["active_step"] = pending[1]["id"] if pending else None
    return refined


def find_step(state: dict[str, Any], step_id: str) -> tuple[dict[str, Any], dict[str, Any]]:
    for batch in state["batches"]:
        for step in batch["steps"]:
            if step["id"] == step_id:
                return batch, step
    raise ReviewError(f"active step not found: {step_id}")


def next_pending(state: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]] | None:
    for batch in state["batches"]:
        for step in batch["steps"]:
            if step["status"] in {"Pending", "Failed", "InProgress"}:
                return batch, step
    return None


def save_state(run_dir: Path, state: dict[str, Any]) -> None:
    state["updated_at"] = utc_now()
    write_json(run_dir / "state.json", state)


def cmd_init(repo: Path, args: argparse.Namespace) -> None:
    if not re.fullmatch(r"CR-\d{3}", args.id):
        raise ReviewError("--id must match CR-NNN")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9-]*", args.slug):
        raise ReviewError("--slug must contain only letters, numbers, and hyphens")
    runs_root = repo / "CodeReview" / "Runs"
    run_dir = runs_root / f"{args.id}-{args.slug}"
    if run_dir.exists():
        raise ReviewError(f"run already exists: {run_dir}")
    branch = args.branch or run(["git", "branch", "--show-current"], cwd=repo).stdout.strip()
    batches = make_batches()
    first_step = batches[0]["steps"][0]
    first_step["status"] = "InProgress"
    first_step["started_at"] = utc_now()
    batches[0]["status"] = "InProgress"
    state = {
        "schema_version": SCHEMA_VERSION,
        "review": {
            "id": args.id,
            "slug": args.slug,
            "status": "Active",
            "objective": "Audit and harden Features 003-019 before Feature 020.",
        },
        "git": {
            "baseline": args.baseline,
            "branch": branch,
            "last_recorded_head": args.baseline,
        },
        "policy": {
            "fix_severities": ["S0", "S1", "S2"],
            "s3": "Fix low-risk local issues; defer the remainder explicitly.",
            "max_inspection_files": 8,
            "max_inspection_lines": 1500,
            "max_fix_findings": 3,
        },
        "active_step": first_step["id"],
        "batches": batches,
        "gates": {},
        "events": [
            {
                "at": utc_now(),
                "kind": "init",
                "detail": f"initialized at baseline {args.baseline}",
            }
        ],
        "created_at": utc_now(),
        "updated_at": utc_now(),
    }
    run_dir.mkdir(parents=True)
    (run_dir / "Batches").mkdir()
    (run_dir / "Evidence" / "output").mkdir(parents=True)
    atomic_write(
        run_dir / "charter.md",
        charter_text(args.id, args.slug, args.baseline, branch),
    )
    atomic_write(run_dir / "execution.md", execution_text())
    atomic_write(run_dir / "decisions.md", "# Decisions\n\nNo decisions recorded.\n")
    write_json(run_dir / "findings.json", {"schema_version": 1, "findings": []})
    save_state(run_dir, state)
    spec_trace.write_seed(repo, run_dir / "traceability.csv")
    render_all(repo, run_dir, state)
    print(f"initialized {run_dir.relative_to(repo)}")


def charter_text(run_id: str, slug: str, baseline: str, branch: str) -> str:
    return f"""# {run_id}: {slug}

## Objective

Audit and harden Features 003-019 before introducing the Asset layer in Feature
020. This review does not implement Feature 020.

## Frozen Baseline

- Commit: `{baseline}`
- Branch: `{branch}`
- Scope: project C++ sources, tests, SCons, CI, demo, validation, roadmap,
  constitution, and Features 003-019 artifacts.

## Policy

- Authority: constitution, clarified spec FR/SC, contracts/plan, roadmap, tasks,
  then code/tests.
- Fix every accepted S0-S2 finding.
- Fix low-risk local S3 findings and defer the rest with an explicit target.
- Controlled public API repair is allowed only with callers, tests, documents,
  and migration notes updated together.
- Do not rewrite historical specifications to hide defects.

## Completion

All requirements are classified with evidence; accepted findings are verified;
required local and three-platform remote gates pass; CodeGraph covers final
HEAD; the `stoner-cr` environment and CLI tests reproduce cleanly; and a
temporary CR-002 initialization succeeds.
"""


def execution_text(batches: list[dict[str, Any]] | None = None) -> str:
    lines = ["# Execution", "", "Execute one `crctl next` packet per session.", ""]
    rendered_batches = batches or make_batches()
    for batch in rendered_batches:
        lines.append(f"## {batch['id']}: {batch['title']}")
        lines.extend(f"- {step['id']}: {step['title']}" for step in batch["steps"])
        lines.append("")
    lines.extend(
        [
            "## Required Gates",
            "",
            "- Debug and Release",
            "- ASan/UBSan after B01 introduces the profiles",
            "- Deterministic and required native validation",
            "- Three-platform GitHub CI at batch boundaries",
            "- Final CodeGraph rebuild and coverage report",
            "",
        ]
    )
    return "\n".join(lines)


def doctor_report(repo: Path) -> tuple[bool, dict[str, Any]]:
    required = read_json(repo / "CodeReview" / "Tools" / "tool-versions.json")["required"]
    python_ok = platform.python_version_tuple()[:2] == tuple(required["python"].split("."))
    environment = os.environ.get("CONDA_DEFAULT_ENV")
    env_ok = environment == required["conda_environment"]
    try:
        scons_version = importlib.metadata.version("scons")
    except importlib.metadata.PackageNotFoundError:
        scons_version = None
    scons_ok = scons_version == required["scons"]
    tools = {
        name: shutil.which(name)
        for name in ("git", "codegraph", "scons", "clang-tidy", "cppcheck", "include-what-you-use")
    }
    git = git_snapshot(repo)
    graph = codegraph_adapter.status(repo) if tools["codegraph"] else {"available": False}
    report = {
        "python": {"actual": platform.python_version(), "ok": python_ok},
        "conda_environment": {"actual": environment, "ok": env_ok},
        "scons": {"actual": scons_version, "ok": scons_ok},
        "tools": tools,
        "git": git,
        "codegraph": graph,
    }
    return python_ok and env_ok and scons_ok and bool(tools["git"]) and bool(tools["codegraph"]), report


def cmd_doctor(repo: Path) -> None:
    ok, report = doctor_report(repo)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if not ok:
        raise ReviewError("doctor found required environment or tool failures")


def cmd_baseline(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    source_files = codegraph_adapter.cpp_inventory(repo)
    source_lines = {}
    for top in ("Source", "Tests"):
        paths = [repo / item for item in source_files if item.startswith(f"{top}/")]
        source_lines[top] = sum(
            len(path.read_text(encoding="utf-8", errors="replace").splitlines())
            for path in paths
        )
    trace_rows = read_csv(run_dir / "traceability.csv")
    violations = architecture_scan.scan(repo)
    baseline = {
        "recorded_at": utc_now(),
        "git": git_snapshot(repo),
        "inventory": {
            "cpp_files": len(source_files),
            "lines": source_lines,
            "requirements": len(trace_rows),
        },
        "architecture_scan": {"violation_count": len(violations), "violations": violations},
        "codegraph": {
            "status": codegraph_adapter.status(repo),
            "cpp_coverage": codegraph_adapter.cpp_coverage(repo),
        },
    }
    write_json(run_dir / "Evidence" / "baseline.json", baseline)
    append_event(state, "baseline", "captured repository baseline")
    save_state(run_dir, state)
    print(json.dumps(baseline["inventory"], indent=2))


def cmd_status(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    snapshot = git_snapshot(repo)
    active = next_pending(state)
    payload = {
        "review": state["review"],
        "active": active[1] if active else None,
        "batch": active[0]["id"] if active else None,
        "git": snapshot,
        "recorded_head": state["git"]["last_recorded_head"],
        "gates": state["gates"],
    }
    print(json.dumps(payload, indent=2, ensure_ascii=False))


def cmd_next(state: dict[str, Any]) -> None:
    pending = next_pending(state)
    if not pending:
        print("No pending step. Run close after all completion gates are satisfied.")
        return
    batch, step = pending
    limits = state["policy"]
    print(
        json.dumps(
            {
                "batch": batch["id"],
                "batch_title": batch["title"],
                "step": step,
                "limits": {
                    "inspection": (
                        f"one domain, {limits['max_inspection_files']} production files, "
                        f"or {limits['max_inspection_lines']} LOC"
                    ),
                    "fix": f"{limits['max_fix_findings']} related findings or one API migration",
                },
                "finish_with": [
                    "record evidence/findings",
                    "crctl complete",
                    "crctl render",
                    "crctl lint",
                    "commit and update handoff",
                ],
            },
            indent=2,
        )
    )


def cmd_start(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    pending = next_pending(state)
    if not pending:
        raise ReviewError("no pending step")
    batch, step = pending
    snapshot = git_snapshot(repo)
    if snapshot["dirty"]:
        raise ReviewError("cannot start a step with unrecorded working tree changes")
    step["status"] = "InProgress"
    step["started_at"] = utc_now()
    batch["status"] = "InProgress"
    state["active_step"] = step["id"]
    state["git"]["last_recorded_head"] = snapshot["head"]
    append_event(state, "start", step["id"])
    save_state(run_dir, state)
    render_all(repo, run_dir, state)
    print(f"started {step['id']}")


def cmd_complete(
    repo: Path, run_dir: Path, state: dict[str, Any], args: argparse.Namespace
) -> None:
    pending = next_pending(state)
    if not pending:
        raise ReviewError("no active or pending step")
    batch, step = pending
    if step["status"] != "InProgress":
        raise ReviewError(f"{step['id']} must be started before completion")
    step["status"] = "Completed"
    step["completed_at"] = utc_now()
    step["evidence"] = args.evidence
    step["commit"] = args.commit
    step["note"] = args.note
    if all(item["status"] == "Completed" for item in batch["steps"]):
        batch["status"] = "Completed"
    upcoming = next_pending(state)
    state["active_step"] = upcoming[1]["id"] if upcoming else None
    state["git"]["last_recorded_head"] = git_snapshot(repo)["head"]
    append_event(state, "complete", step["id"])
    save_state(run_dir, state)
    render_all(repo, run_dir, state)
    print(f"completed {step['id']}; next: {state['active_step'] or 'close'}")


def cmd_fail(run_dir: Path, state: dict[str, Any], reason: str) -> None:
    pending = next_pending(state)
    if not pending:
        raise ReviewError("no step to fail")
    batch, step = pending
    step["status"] = "Failed"
    step["note"] = reason
    batch["status"] = "Blocked"
    append_event(state, "fail", f"{step['id']}: {reason}")
    save_state(run_dir, state)
    print(f"failed {step['id']}: {reason}")


def cmd_recover(repo: Path, state: dict[str, Any]) -> None:
    snapshot = git_snapshot(repo)
    pending = next_pending(state)
    report = {
        "recorded_head": state["git"]["last_recorded_head"],
        "current_git": snapshot,
        "head_drift": snapshot["head"] != state["git"]["last_recorded_head"],
        "unrecorded_diff": snapshot["changes"],
        "active_step": pending[1] if pending else None,
        "next_command": (
            f"python CodeReview/Tools/crctl.py next --id {state['review']['id']}"
            if not snapshot["dirty"]
            else "classify and record the working tree diff before advancing"
        ),
    }
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if snapshot["dirty"]:
        raise ReviewError("recovery blocked by unrecorded working tree changes")


def cmd_refine(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    snapshot = git_snapshot(repo)
    if snapshot["dirty"]:
        raise ReviewError("cannot refine packets with unrecorded working tree changes")
    refined = refine_pending_batches(state)
    if refined:
        append_event(state, "refine", f"refined pending batches: {', '.join(refined)}")
        atomic_write(run_dir / "execution.md", execution_text(state["batches"]))
        save_state(run_dir, state)
        render_all(repo, run_dir, state)
    print(
        "refined pending batches: " + ", ".join(refined)
        if refined
        else "no pristine pending batches required refinement"
    )


def cmd_trace(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    output = run_dir / "traceability.csv"
    existing = read_csv(output)
    if existing and any(row.get("classification") != "Unclassified" for row in existing):
        raise ReviewError("refusing to overwrite classified traceability records")
    count = spec_trace.write_seed(repo, output)
    append_event(state, "trace", f"seeded {count} FR/SC records")
    save_state(run_dir, state)
    print(f"seeded {count} traceability records")


def findings_document(run_dir: Path) -> dict[str, Any]:
    return read_json(run_dir / "findings.json")


def next_finding_id(document: dict[str, Any], review_id: str, batch: str) -> str:
    prefix = f"{review_id.replace('-', '')}-{batch}-F"
    numbers = [
        int(item["id"].removeprefix(prefix))
        for item in document["findings"]
        if item["id"].startswith(prefix)
    ]
    return f"{prefix}{(max(numbers, default=0) + 1):03d}"


def require_finding(document: dict[str, Any], finding_id: str) -> dict[str, Any]:
    for finding in document["findings"]:
        if finding["id"] == finding_id:
            return finding
    raise ReviewError(f"unknown finding: {finding_id}")


def transition(finding: dict[str, Any], target: str, note: str) -> None:
    current = finding["status"]
    if target not in FINDING_TRANSITIONS[current]:
        raise ReviewError(f"invalid finding transition: {current} -> {target}")
    finding["status"] = target
    finding.setdefault("history", []).append(
        {"at": utc_now(), "from": current, "to": target, "note": note}
    )


def cmd_finding(
    repo: Path,
    run_dir: Path,
    state: dict[str, Any],
    args: argparse.Namespace,
) -> None:
    document = findings_document(run_dir)
    action = args.finding_command
    if action == "add":
        if not any(batch["id"] == args.batch for batch in state["batches"]):
            raise ReviewError(f"unknown batch: {args.batch}")
        finding_id = next_finding_id(document, state["review"]["id"], args.batch)
        document["findings"].append(
            {
                "id": finding_id,
                "batch": args.batch,
                "severity": args.severity,
                "status": "Open",
                "title": args.title,
                "requirement": args.requirement,
                "location": args.location,
                "evidence": args.evidence,
                "impact": args.impact,
                "resolution": "",
                "verification": "",
                "commit": "",
                "deferred_target": "",
                "history": [],
                "created_at": utc_now(),
            }
        )
        print(finding_id)
    else:
        finding = require_finding(document, args.finding_id)
        if action == "triage":
            if finding["status"] == "Open":
                transition(finding, "Triaged", args.note)
            transition(finding, args.disposition, args.note)
        elif action == "fix":
            transition(finding, "Fixed", args.note)
            finding["resolution"] = args.note
            finding["commit"] = args.commit
        elif action == "verify":
            transition(finding, "Verified", args.note)
            finding["verification"] = args.note
        elif action == "defer":
            if finding["severity"] in {"S0", "S1", "S2"}:
                raise ReviewError("S0-S2 findings cannot be deferred under CR-001 policy")
            if finding["status"] == "Open":
                transition(finding, "Triaged", "triaged for deferral")
            transition(finding, "Deferred", args.reason)
            finding["deferred_target"] = args.target
            finding["resolution"] = args.reason
    write_json(run_dir / "findings.json", document)
    append_event(state, "finding", f"{action} finding")
    save_state(run_dir, state)
    render_all(repo, run_dir, state)


def render_all(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    findings = findings_document(run_dir).get("findings", [])
    completed = sum(
        step["status"] == "Completed"
        for batch in state["batches"]
        for step in batch["steps"]
    )
    total = sum(len(batch["steps"]) for batch in state["batches"])
    pending = next_pending(state)
    progress = [
        f"# {state['review']['id']} Progress",
        "",
        f"- Status: {state['review']['status']}",
        f"- Completed steps: {completed}/{total}",
        f"- Baseline: `{state['git']['baseline']}`",
        f"- Recorded HEAD: `{state['git']['last_recorded_head']}`",
        f"- Active step: {pending[1]['id'] if pending else 'none'}",
        f"- Open findings: {sum(item['status'] not in {'Verified', 'Deferred', 'Rejected'} for item in findings)}",
        "",
        "## Batches",
        "",
    ]
    progress.extend(
        f"- {batch['id']} {batch['title']}: {batch['status']}" for batch in state["batches"]
    )
    progress.extend(
        [
            "",
            "## Next Command",
            "",
            f"`conda run -n stoner-cr python CodeReview/Tools/crctl.py next --id {state['review']['id']}`",
            "",
        ]
    )
    atomic_write(run_dir / "progress.md", "\n".join(progress))

    finding_lines = ["# Findings", ""]
    if not findings:
        finding_lines.append("No findings recorded.")
    for finding in findings:
        finding_lines.extend(
            [
                f"## {finding['id']}: {finding['title']}",
                "",
                f"- Severity: {finding['severity']}",
                f"- Status: {finding['status']}",
                f"- Requirement: {finding['requirement']}",
                f"- Location: `{finding['location']}`",
                f"- Impact: {finding['impact']}",
                f"- Evidence: {finding['evidence']}",
                f"- Resolution: {finding['resolution'] or 'pending'}",
                f"- Verification: {finding['verification'] or 'pending'}",
                f"- Commit: `{finding['commit'] or 'pending'}`",
                "",
            ]
        )
    atomic_write(run_dir / "findings.md", "\n".join(finding_lines))

    snapshot = git_snapshot(repo)
    handoff = [
        f"# {state['review']['id']} Handoff",
        "",
        f"- Base: `{state['git']['baseline']}`",
        f"- Current HEAD: `{snapshot['head']}`",
        f"- Branch: `{snapshot['branch']}`",
        f"- Worktree: `{repo}`",
        f"- Active batch/step: `{pending[0]['id'] if pending else 'none'}` / `{pending[1]['id'] if pending else 'none'}`",
        f"- Working tree dirty: {snapshot['dirty']}",
        f"- Open findings: {', '.join(item['id'] for item in findings if item['status'] not in {'Verified', 'Deferred', 'Rejected'}) or 'none'}",
        f"- Latest gates: {json.dumps(state['gates'], sort_keys=True)}",
        "",
        "## Recovery",
        "",
        f"Run `conda run -n stoner-cr python CodeReview/Tools/crctl.py recover --id {state['review']['id']}`.",
        "Do not advance when recover reports an unrecorded diff.",
        "",
        "## Next Command",
        "",
        f"`conda run -n stoner-cr python CodeReview/Tools/crctl.py next --id {state['review']['id']}`",
        "",
    ]
    atomic_write(run_dir / "handoff.md", "\n".join(handoff))


def lint_run(repo: Path, run_dir: Path, state: dict[str, Any]) -> list[str]:
    errors: list[str] = []
    if state.get("schema_version") != SCHEMA_VERSION:
        errors.append("unsupported state schema")
    step_ids = [
        step["id"] for batch in state.get("batches", []) for step in batch.get("steps", [])
    ]
    if len(step_ids) != len(set(step_ids)):
        errors.append("duplicate step IDs")
    document = findings_document(run_dir)
    finding_ids = [item.get("id") for item in document.get("findings", [])]
    if len(finding_ids) != len(set(finding_ids)):
        errors.append("duplicate finding IDs")
    expected_pattern = re.compile(
        rf"^{re.escape(state['review']['id'].replace('-', ''))}-B\d{{2}}-F\d{{3}}$"
    )
    for finding in document.get("findings", []):
        if not expected_pattern.fullmatch(finding.get("id", "")):
            errors.append(f"invalid finding ID: {finding.get('id')}")
        if finding.get("severity") not in SEVERITIES:
            errors.append(f"invalid severity: {finding.get('id')}")
        if finding.get("status") not in FINDING_STATES:
            errors.append(f"invalid status: {finding.get('id')}")
        if finding.get("status") == "Deferred" and not finding.get("deferred_target"):
            errors.append(f"deferred finding lacks target: {finding.get('id')}")
        if finding.get("status") == "Verified" and not finding.get("verification"):
            errors.append(f"verified finding lacks evidence: {finding.get('id')}")
    trace_rows = read_csv(run_dir / "traceability.csv")
    trace_ids = [row.get("trace_id") for row in trace_rows]
    if len(trace_ids) != len(set(trace_ids)):
        errors.append("duplicate traceability IDs")
    required_files = (
        "charter.md",
        "execution.md",
        "state.json",
        "progress.md",
        "handoff.md",
        "traceability.csv",
        "findings.json",
        "findings.md",
        "decisions.md",
    )
    errors.extend(name for name in required_files if not (run_dir / name).is_file())
    return errors


def cmd_gate(repo: Path, run_dir: Path, state: dict[str, Any], profile: str) -> None:
    result = gate_runner.execute(repo, profile)
    state["gates"][profile] = result
    append_event(state, "gate", f"{profile}: {'pass' if result['passed'] else 'fail'}")
    save_state(run_dir, state)
    write_json(run_dir / "Evidence" / f"gate-{profile}.json", result)
    print(json.dumps(result, indent=2))
    if not result["passed"]:
        raise ReviewError(f"gate failed: {profile}")


def cmd_close(repo: Path, run_dir: Path, state: dict[str, Any]) -> None:
    errors = lint_run(repo, run_dir, state)
    findings = findings_document(run_dir)["findings"]
    accepted_open = [
        item["id"]
        for item in findings
        if item["status"] in {"Accepted", "Fixed"}
    ]
    if accepted_open:
        errors.append(f"accepted findings not verified: {', '.join(accepted_open)}")
    traces = read_csv(run_dir / "traceability.csv")
    incomplete = [row["trace_id"] for row in traces if row["classification"] == "Unclassified"]
    if incomplete:
        errors.append(f"{len(incomplete)} traceability records remain unclassified")
    unfinished = [
        step["id"]
        for batch in state["batches"]
        for step in batch["steps"]
        if batch["id"] != "B10" and step["status"] != "Completed"
    ]
    if unfinished:
        errors.append(f"unfinished review steps: {', '.join(unfinished)}")
    if errors:
        raise ReviewError("cannot close review:\n- " + "\n- ".join(errors))
    state["review"]["status"] = "Closed"
    append_event(state, "close", "completion conditions satisfied")
    save_state(run_dir, state)
    render_all(repo, run_dir, state)
    print(f"closed {state['review']['id']}")


def main() -> int:
    args = parse_args()
    try:
        repo = find_repo_root()
        if args.command == "init":
            cmd_init(repo, args)
            return 0
        run_dir, state = load_run(repo, command_run_id(args))
        if args.command == "doctor":
            cmd_doctor(repo)
        elif args.command == "baseline":
            cmd_baseline(repo, run_dir, state)
        elif args.command == "status":
            cmd_status(repo, run_dir, state)
        elif args.command == "next":
            cmd_next(state)
        elif args.command == "start":
            cmd_start(repo, run_dir, state)
        elif args.command == "complete":
            cmd_complete(repo, run_dir, state, args)
        elif args.command == "fail":
            cmd_fail(run_dir, state, args.reason)
        elif args.command == "recover":
            cmd_recover(repo, state)
        elif args.command == "refine":
            cmd_refine(repo, run_dir, state)
        elif args.command == "trace":
            cmd_trace(repo, run_dir, state)
        elif args.command == "finding":
            cmd_finding(repo, run_dir, state, args)
        elif args.command == "render":
            render_all(repo, run_dir, state)
        elif args.command == "gate":
            cmd_gate(repo, run_dir, state, args.profile)
        elif args.command == "lint":
            errors = lint_run(repo, run_dir, state)
            if errors:
                raise ReviewError("lint failed:\n- " + "\n- ".join(errors))
            print("review state lint passed")
        elif args.command == "close":
            cmd_close(repo, run_dir, state)
        return 0
    except (ReviewError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
