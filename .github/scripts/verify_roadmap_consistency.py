#!/usr/bin/env python3
"""Read-only roadmap/spec/task consistency scan with normalized JSON output."""

from __future__ import annotations

import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
ROADMAP = ROOT / "doc/roadmap.md"
ROADMAP_SPEC = ROOT / "specs/002-engine-development-roadmap/spec.md"
ROADMAP_TASKS = ROOT / "specs/002-engine-development-roadmap/tasks.md"
FEATURE_SPEC = ROOT / "specs/029-hdr-output-transform/spec.md"
FEATURE_PLAN = ROOT / "specs/029-hdr-output-transform/plan.md"
FEATURE_TASKS = ROOT / "specs/029-hdr-output-transform/tasks.md"

RUNTIME_PHASES = set(range(3, 42))
FROZEN_DEPENDENCIES = {
    29: (13, 15, 18, 19, 27, 28),
    30: (4, 13, 15, 17, 19, 28, 29),
    31: (24, 25, 26, 28),
    39: (13, 19, 30),
}


def slugify(heading: str) -> str:
    value = re.sub(r"[^\w\- ]", "", heading.lower(), flags=re.UNICODE)
    return value.replace(" ", "-")


def numbered_set(matches: list[str]) -> set[int]:
    return {int(value) for value in matches}


def require_exact_runtime_set(label: str, values: list[str],
                              findings: list[str]) -> None:
    numbers = [int(value) for value in values]
    if set(numbers) != RUNTIME_PHASES:
        missing = sorted(RUNTIME_PHASES - set(numbers))
        extra = sorted(set(numbers) - RUNTIME_PHASES)
        findings.append(f"{label}: runtime set mismatch missing={missing} extra={extra}")
    duplicates = sorted(number for number in set(numbers)
                        if numbers.count(number) != 1)
    if duplicates:
        findings.append(f"{label}: duplicate phases {duplicates}")


def parse_dependencies(value: str) -> tuple[int, ...]:
    return tuple(int(item) for item in re.findall(r"\b\d{3}\b", value))


def check_task_sequence(path: Path, findings: list[str]) -> int:
    text = path.read_text(encoding="utf-8")
    ids = [int(value) for value in re.findall(
        r"^- \[[ xX]\] T(\d{3})\b", text, flags=re.MULTILINE)]
    if not ids:
        findings.append(f"{path.relative_to(ROOT)}: no task records")
        return 0
    expected = list(range(1, max(ids) + 1))
    if sorted(ids) != expected:
        findings.append(
            f"{path.relative_to(ROOT)}: task IDs are not unique contiguous T001-T{max(ids):03d}")
    return len(ids)


def check_requirement_sequence(prefix: str, maximum: int, text: str,
                               findings: list[str]) -> None:
    ids = [int(value) for value in re.findall(
        rf"^- \*\*{prefix}-(\d{{3}})\*\*:", text, flags=re.MULTILINE)]
    if ids != list(range(1, maximum + 1)):
        findings.append(f"Feature 029 {prefix} identifiers are not contiguous 001-{maximum:03d}")


def main() -> int:
    findings: list[str] = []
    roadmap = ROADMAP.read_text(encoding="utf-8")
    roadmap_spec = ROADMAP_SPEC.read_text(encoding="utf-8")
    feature_spec = FEATURE_SPEC.read_text(encoding="utf-8")
    feature_plan = FEATURE_PLAN.read_text(encoding="utf-8")

    toc = re.findall(
        r"^\s+- \[Phase (\d{3}) - ([^]]+)\]\((#[^)]+)\)$",
        roadmap, flags=re.MULTILINE)
    details = re.findall(r"^### Phase (\d{3}) — (.+)$", roadmap,
                         flags=re.MULTILINE)
    overview = re.findall(
        r"^\| (\d{3}) \| ([^|]+) \| ([^|]+) \| ([^|]+) \| [^|]+ \| [^|]+ \| [^|]+ \|$",
        roadmap, flags=re.MULTILINE)
    graph_nodes = re.findall(r"\bP(\d{3})\[[^\]]+\]", roadmap)

    require_exact_runtime_set("TOC", [item[0] for item in toc], findings)
    require_exact_runtime_set("phase details", [item[0] for item in details], findings)
    require_exact_runtime_set("overview", [item[0] for item in overview], findings)
    require_exact_runtime_set(
        "dependency graph", [value for value in graph_nodes if int(value) >= 3],
        findings)

    detail_titles = {int(number): title for number, title in details}
    for number, title, anchor in toc:
        phase = int(number)
        if detail_titles.get(phase) != title:
            findings.append(f"Phase {phase:03d}: TOC/detail title mismatch")
        expected_anchor = "#" + slugify(f"Phase {number} — {title}")
        if anchor != expected_anchor:
            findings.append(
                f"Phase {phase:03d}: anchor {anchor} != {expected_anchor}")

    heading_anchors = {slugify(value) for value in re.findall(
        r"^#{1,6} (.+)$", roadmap, flags=re.MULTILINE)}
    for target in re.findall(r"\]\(#([^)]+)\)", roadmap):
        if target not in heading_anchors:
            findings.append(f"roadmap internal anchor has no heading: #{target}")

    overview_dependencies = {
        int(number): parse_dependencies(dependencies)
        for number, _title, _layer, dependencies in overview
    }
    detail_dependencies: dict[int, tuple[int, ...]] = {}
    for match in re.finditer(r"^### Phase (\d{3}) — .+$", roadmap,
                             flags=re.MULTILINE):
        phase = int(match.group(1))
        next_match = re.search(r"^### Phase \d{3} — .+$",
                               roadmap[match.end():], flags=re.MULTILINE)
        end = match.end() + next_match.start() if next_match else len(roadmap)
        section = roadmap[match.end():end]
        dep_match = re.search(r"^\*\*Dependencies\*\*: (.+)$", section,
                              flags=re.MULTILINE)
        if not dep_match:
            findings.append(f"Phase {phase:03d}: missing detail dependencies")
            continue
        detail_dependencies[phase] = parse_dependencies(dep_match.group(1))

    graph_dependencies = {phase: [] for phase in range(1, 42)}
    for source, target in re.findall(r"\bP(\d{3})\s*-->\s*P(\d{3})\b", roadmap):
        graph_dependencies[int(target)].append(int(source))

    for phase in sorted(RUNTIME_PHASES):
        overview_deps = overview_dependencies.get(phase, ())
        detail_deps = detail_dependencies.get(phase, ())
        graph_deps = tuple(sorted(graph_dependencies.get(phase, [])))
        if overview_deps != detail_deps:
            findings.append(f"Phase {phase:03d}: overview/detail dependencies differ")
        if tuple(sorted(overview_deps)) != graph_deps:
            findings.append(f"Phase {phase:03d}: overview/graph dependencies differ")
        if any(dependency >= phase for dependency in overview_deps):
            findings.append(f"Phase {phase:03d}: dependency is not backward-only")

    for phase, expected in FROZEN_DEPENDENCIES.items():
        if overview_dependencies.get(phase) != expected:
            findings.append(
                f"Phase {phase:03d}: frozen dependencies differ from {expected}")

    version = re.search(r"\*\*Version\*\*: ([0-9.]+)", roadmap)
    last_updated = re.search(r"\*\*Last Updated\*\*: (\d{4}-\d{2}-\d{2})", roadmap)
    change_versions = re.findall(r"^\| \d{4}-\d{2}-\d{2} \| ([0-9.]+) \|",
                                 roadmap, flags=re.MULTILINE)
    if not version or not change_versions or version.group(1) != change_versions[0]:
        findings.append("roadmap header version does not match newest change-log version")
    if not last_updated or not re.search(
            rf"^\| {re.escape(last_updated.group(1))} \|",
            roadmap, flags=re.MULTILINE):
        findings.append("roadmap Last Updated has no matching change-log entry")

    feature_029_dependencies = re.search(
        r"^\*\*Dependencies\*\*: ([0-9, ]+)$",
        feature_plan[feature_plan.find("## Technical Context"):],
        flags=re.MULTILINE)
    if feature_029_dependencies and parse_dependencies(
            feature_029_dependencies.group(1)) != FROZEN_DEPENDENCIES[29]:
        findings.append("Feature 029 plan dependency list drifted")

    check_requirement_sequence("FR", 46, feature_spec, findings)
    check_requirement_sequence("SC", 16, feature_spec, findings)
    roadmap_task_count = check_task_sequence(ROADMAP_TASKS, findings)
    feature_task_count = check_task_sequence(FEATURE_TASKS, findings)

    authority_paths = [
        ROADMAP,
        ROOT / "AGENTS.md",
        ROOT / "specs/002-engine-development-roadmap/spec.md",
        ROOT / "specs/002-engine-development-roadmap/plan.md",
        ROOT / "specs/002-engine-development-roadmap/research.md",
        ROOT / "specs/002-engine-development-roadmap/data-model.md",
        ROOT / "specs/002-engine-development-roadmap/quickstart.md",
        ROOT / "specs/002-engine-development-roadmap/contracts/roadmap-phase-schema.md",
        ROOT / "specs/028-production-content-acceptance/spec.md",
        ROOT / "specs/028-production-content-acceptance/research.md",
        ROOT / "doc/028-production-content-acceptance.html",
    ]
    stale_patterns = (
        r"(?:Feature|Phase) 029\s*(?:[-—:]\s*)?(?:Asset:\s*)?Meshlet",
        r"(?:Feature|Phase) 030\s*(?:[-—:]\s*)?(?:Asset:\s*)?Streaming",
        r"(?:Feature|Phase) 039\s*(?:[-—:]\s*)?(?:Renderer:\s*)?Hybrid GI",
        r"Meshlet(?: Derived Data)?\s*(?:is|at|as)?\s*(?:Feature|Phase) 029",
    )
    for path in authority_paths:
        text = path.read_text(encoding="utf-8")
        for pattern in stale_patterns:
            if re.search(pattern, text, flags=re.IGNORECASE):
                findings.append(
                    f"{path.relative_to(ROOT)}: stale future-phase reference matches {pattern}")

    temporal_sources = roadmap + roadmap_spec
    for term in ("motion-vector", "jitter", "history", "reprojection"):
        if not re.search(rf"Feature 030[^\n]{{0,400}}{re.escape(term)}|{re.escape(term)}[^\n]{{0,400}}Feature 030",
                         temporal_sources, flags=re.IGNORECASE):
            findings.append(f"Feature 039 temporal reuse lacks term: {term}")

    if "EDRMetadata=nil" not in feature_spec or "BGR10A2Unorm" not in feature_spec:
        findings.append("Feature 029 Apple PQ/EDR governance is incomplete")

    result = {
        "schema": "stoner.roadmap-consistency-scan",
        "schemaVersion": 1,
        "status": "passed" if not findings else "failed",
        "findingCount": len(findings),
        "checks": {
            "runtimePhaseRange": "003-041",
            "runtimePhaseCount": len(RUNTIME_PHASES),
            "roadmapTaskCount": roadmap_task_count,
            "feature029TaskCount": feature_task_count,
            "feature029RequirementCount": 46,
            "feature029SuccessCriterionCount": 16,
            "frozenDependencyPhases": ["029", "030", "031", "039"],
            "internalAnchorCount": len(re.findall(r"\]\(#[^)]+\)", roadmap)),
            "staleReferenceFileCount": len(authority_paths),
        },
        "findings": sorted(set(findings)),
    }
    print(json.dumps(result, sort_keys=True, separators=(",", ":")))
    return 0 if not findings else 1


if __name__ == "__main__":
    raise SystemExit(main())
