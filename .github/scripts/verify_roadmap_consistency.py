#!/usr/bin/env python3
"""Read-only, mutation-tested Roadmap 3.1 structural and semantic consistency."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
RUNTIME_PHASES = set(range(3, 54))
FUTURE_PHASES = set(range(30, 54))
COMPLETED_DETAIL_SHA256 = "1d1adc6036ae8c2786053c6414f110a2ed76d5989dc7c53d97db2d54c2dc64fd"
FROZEN_DEPENDENCIES = {29: (13, 15, 18, 19, 27, 28),
                       30: (4, 8, 13, 15, 16, 17, 18, 19, 27, 28, 29),
                       31: (4, 13, 15, 17, 19, 28, 29, 30),
                       43: (24, 25, 26, 28), 46: (13, 19, 31, 40, 41)}
OLD_TO_NEW = {"030":"031","031":"043","032":"044","033":"045","034":"051","035":"052","036":"053","037":"047","038":"048","039":"046","040":"049","041":"050"}
PREVIOUS_TO_CURRENT = {f"{n:03d}": f"{n + 1:03d}" for n in range(30, 53)}


def slugify(heading: str) -> str:
    return re.sub(r"[^\w\- ]", "", heading.lower()).replace(" ", "-")


def deps(value: str) -> tuple[int, ...]:
    return tuple(int(v) for v in re.findall(r"\b\d{3}\b", value))


def result(checks: dict, findings: list[str]) -> dict:
    findings = sorted(set(findings))
    return {"schema": "stoner.roadmap-consistency-scan", "schemaVersion": 2,
            "status": "failed" if findings else "passed", "findingCount": len(findings),
            "checks": checks, "findings": findings}


def scan(root: Path = ROOT) -> dict:
    findings: list[str] = []
    checks: dict = {"runtimePhaseRange": "003-053", "runtimePhaseCount": 51,
                    "futurePhaseCount": 24, "frozenDependencyPhases": ["029", "030", "031", "043", "046"]}

    def require(ok: bool, message: str) -> None:
        if not ok:
            findings.append(message)

    def exact_set(label: str, numbers: list[int], expected: set[int]) -> None:
        require(set(numbers) == expected, f"{label}: runtime set mismatch")
        require(len(numbers) == len(set(numbers)), f"{label}: duplicate phase identifiers")

    def sequence(label: str, text: str, pattern: str, maximum: int | None = None) -> int:
        values = [int(v) for v in re.findall(pattern, text, re.M)]
        end = maximum if maximum is not None else max(values, default=0)
        require(bool(values) and values == list(range(1, end + 1)),
                f"{label}: identifiers must be unique and contiguous")
        return len(values)

    governance = root / "specs/002-engine-development-roadmap"
    try:
        roadmap = (root / "doc/roadmap.md").read_text(encoding="utf-8")
        agents = (root / "AGENTS.md").read_text(encoding="utf-8")
        docs = {p.relative_to(governance).as_posix(): p.read_text(encoding="utf-8")
                for p in governance.rglob("*.md")}
        for name in ("spec.md", "plan.md", "tasks.md", "research.md", "data-model.md",
                     "quickstart.md", "contracts/roadmap-phase-schema.md",
                     "checklists/requirements.md", "migration-3.0.md", "migration-3.0.1.md", "migration-3.1.md"):
            require(name in docs, f"missing governance document: {name}")
        index = json.loads((governance / "phase-index.json").read_text(encoding="utf-8"))
        feature = {n: (root / "specs/029-hdr-output-transform" / n).read_text(encoding="utf-8")
                   for n in ("spec.md", "plan.md", "tasks.md")}
        entries = index["futurePhases"]
        if not isinstance(entries, list):
            raise ValueError("futurePhases must be an array")
        expected = {int(e["number"]): e for e in entries}
        for e in entries:
            if not isinstance(e["name"], str) or not isinstance(e["layer"], str) or not isinstance(e["dependencies"], list):
                raise ValueError("invalid future phase owner/title/dependencies")
            tuple(int(d) for d in e["dependencies"])
    except (OSError, KeyError, TypeError, ValueError) as error:
        return result(checks, findings + [f"required roadmap input: {error}"])

    require(index.get("schema") == "stoner.roadmap-phase-index" and index.get("schemaVersion") == 1
            and index.get("roadmapVersion") == "3.1.0" and index.get("activeRuntimeRange") == "003-053"
            and index.get("completedRuntimeRange") == "003-029" and index.get("nextPhase") == "030"
            and index.get("oldToNew") == OLD_TO_NEW, "phase index identity/range/migration mismatch")
    exact_set("phase index", [int(e["number"]) for e in entries], FUTURE_PHASES)
    for number, e in expected.items():
        track = "complete-renderer" if number <= 42 else "optional-backend" if number >= 51 else "advanced-geometry-lighting"
        require(e.get("track") == track, f"Phase {number:03d}: index track mismatch")

    toc = re.findall(r"^\s+- \[Phase (\d{3}) - ([^]]+)\]\((#[^)]+)\)$", roadmap, re.M)
    details = list(re.finditer(r"^### Phase (\d{3}) — (.+)$", roadmap, re.M))
    overview = re.findall(r"^\| (\d{3}) \| ([^|]+) \| ([^|]+) \| ([^|]+) \| ([^|]+) \| ([^|]+) \| ([^|]+) \|$", roadmap, re.M)
    nodes = [int(n) for n in re.findall(r"\bP(\d{3})\[[^\]]+\]", roadmap) if int(n) >= 3]
    for label, values in (("TOC", [int(t[0]) for t in toc]), ("details", [int(m[1]) for m in details]),
                          ("overview", [int(t[0]) for t in overview]), ("graph", nodes)):
        exact_set(label, values, RUNTIME_PHASES)
        if label != "graph":
            require(values == sorted(values), f"{label}: phase order is not increasing")
    titles = {int(m[1]): m[2] for m in details}
    sections: dict[int, str] = {}
    for i, m in enumerate(details):
        end = details[i + 1].start() if i + 1 < len(details) else roadmap.find("\n## Complete Rendering Pipeline Layout", m.end())
        sections[int(m[1])] = roadmap[m.start():end if end >= 0 else len(roadmap)]
    table_deps = {int(row[0]): deps(row[3]) for row in overview}
    detail_deps = {}
    for number, section in sections.items():
        match = re.search(r"^\*\*Dependencies\*\*: (.+)$", section, re.M)
        detail_deps[number] = deps(match[1]) if match else ()
        require(bool(match), f"Phase {number:03d}: missing dependencies")
        fields = ["**Layer**:", "**Complexity**:", "**Critical Path**:", "#### Scope",
                  "#### Key Deliverables", "#### What's Excluded", "#### Speckit Prompt"]
        if number >= 30:
            fields += ["#### Delivery Milestones"]
            prompt = re.search(r"#### Speckit Prompt\s+\x60{3}text\s+(.+?)\s+\x60{3}", section, re.S)
            require(bool(prompt) and prompt[1].startswith("Implement ") and not re.search("see above", prompt[1], re.I),
                    f"Phase {number:03d}: prompt must be self-contained")
            prompt_deps = re.search(r"on Features ([\d, ]+)\.", prompt[1]) if prompt else None
            require(bool(prompt_deps) and deps(prompt_deps[1]) == detail_deps[number],
                    f"Phase {number:03d}: prompt dependencies differ from detail")
            require(len(re.findall(r"^- M\d:", section, re.M)) >= 2, f"Phase {number:03d}: missing bounded Delivery Milestones")
        for field in fields:
            require(field in section, f"Phase {number:03d}: missing {field}")
    for number, title, anchor in toc:
        require(titles.get(int(number)) == title, f"Phase {number}: TOC/detail owner/title mismatch")
        require(anchor == "#" + slugify(f"Phase {number} — {title}"), f"Phase {number}: TOC anchor mismatch")
    anchors = {slugify(h) for h in re.findall(r"^#{1,6} (.+)$", roadmap, re.M)}
    for target in re.findall(r"\]\(#([^)]+)\)", roadmap):
        require(target in anchors, f"roadmap internal anchor missing: {target}")
    checks["internalAnchorCount"] = len(re.findall(r"\]\(#[^)]+\)", roadmap))

    edges = [(int(a), int(b)) for a, b in re.findall(r"\bP(\d{3})\s*-->\s*P(\d{3})\b", roadmap)]
    graph_deps = {n: [] for n in RUNTIME_PHASES}
    require(len(edges) == len(set(edges)), "duplicate dependency graph edges")
    for source, target in edges:
        if source not in RUNTIME_PHASES | {1} or target not in RUNTIME_PHASES:
            findings.append(f"unknown graph dependency node: {source:03d}->{target:03d}")
        else:
            graph_deps[target].append(source)
    for number in sorted(RUNTIME_PHASES):
        declared = table_deps.get(number, ())
        require(declared == detail_deps.get(number, ()), f"Phase {number:03d}: overview/detail dependencies differ")
        require(tuple(sorted(declared)) == tuple(sorted(graph_deps[number])), f"Phase {number:03d}: overview/graph dependencies differ")
        require(len(declared) == len(set(declared)) and all(d < number and d in RUNTIME_PHASES | {1} for d in declared),
                f"Phase {number:03d}: dependencies are duplicate, unknown or not backward-only")
    for number, frozen in FROZEN_DEPENDENCIES.items():
        require(table_deps.get(number) == frozen, f"Phase {number:03d}: protected dependencies differ from {frozen}")
    for row in overview:
        number = int(row[0])
        if number in expected:
            e = expected[number]
            require((row[1].strip(), row[2].strip()) == (e["name"], e["layer"])
                    and titles.get(number) == f"{e['layer']}: {e['name']}", f"Phase {number:03d}: index owner/title mismatch")
            require(table_deps.get(number) == tuple(int(d) for d in e["dependencies"]), f"Phase {number:03d}: index dependencies mismatch")
    start, end = roadmap.find("### Phase 003 —"), roadmap.find("### Phase 030 —")
    require(start >= 0 and end > start and hashlib.sha256(roadmap[start:end].encode()).hexdigest() == COMPLETED_DETAIL_SHA256,
            "completed phase detail digest changed (003-029 pre-amendment snapshot)")

    def requires(number: int, dependency: int) -> bool:
        seen, pending = set(), list(table_deps.get(number, ()))
        while pending:
            value = pending.pop()
            if value == dependency:
                return True
            if value not in seen:
                seen.add(value)
                pending.extend(table_deps.get(value, ()))
        return False

    for number in (33, 35, 36, 37, 38, 39, 40, 46, 48, 50):
        require(requires(number, 31), f"Phase {number:03d}: missing shared temporal dependencies on 031")
    for number in (39, 40, 44, 46):
        require(requires(number, 33), f"Phase {number:03d}: missing shared depth dependencies on 033")
    for number in range(30, 41):
        require(not requires(number, 41), f"Phase {number:03d}: full profiling must follow rendering effects")
    require(41 in table_deps.get(42, ()), "Phase 042: integrated acceptance must explicitly require full profiling 041")
    for number in range(30, 41):
        require(requires(41, number), f"Phase 041: profiling must cover completed effect {number:03d}")
    for number in range(31, 41):
        require(requires(number, 30), f"Phase {number:03d}: missing interactive lab dependency on 030")
    require(index.get("previousRoadmapVersion") == "3.0.1"
            and index.get("previousToCurrent") == PREVIOUS_TO_CURRENT, "interactive migration identity mismatch")
    require(expected.get(30, {}).get("name") == "Interactive Rendering Lab & ImGui Integration"
            and expected.get(30, {}).get("layer") == "Application", "Phase 030: next phase must be the interactive lab")
    semantics = [
        (30, "pinned Dear ImGui revision behind private adapters", "private ImGui integration"),
        (30, "UI capture arbitration", "UI input arbitration"),
        (30, "must not require synchronous CPU readback every frame", "interactive native presentation"),
        (30, "Display-linear UI composition after scene post-processing", "HDR UI composition"),
        (30, "UI reference-white brightness", "UI reference white"),
        (30, "UI bypasses scene exposure, TAA, DOF, motion blur and bloom", "UI effect isolation"),
        (30, "Formal scene captures default to UI disabled", "UI-free scene evidence"),
        (30, "maintainer hands-on navigation/control review", "hands-on acceptance"),

        (33, "Use VarianceShadowMaps for that technique; reserve VirtualShadowMaps", "shadow terminology separation"),
        (33, "SceneDepthPyramid", "shared depth pyramid"),
        (39, "Retain Feature 032 shadow maps/CSM", "virtual-shadow fallback"),
        (39, "not a Nanite-dependent", "bounded virtual-shadow scope"),
        (35, "without applying the same extinction twice", "fog/atmosphere composition"),
        (36, "before affected scene lighting", "cloud-shadow ordering"),
        (37, "manual exposure remains", "deterministic manual exposure"),
        (44, "RHI/backend indirect", "explicit indirect-command responsibility"),
        (49, "dynamic radiance must not be serialized", "runtime surface-cache ownership"),
        (41, "Unsupported timing reports unavailable", "GPU timing capability"),
        (42, "indoor/outdoor", "integrated quality gate")]
    for number, term, label in semantics:
        scope = sections.get(number, "").split("#### Speckit Prompt")[0]
        require(term.lower() in scope.lower(), f"Phase {number:03d}: missing {label}")
    require("## Complete Rendering Pipeline Layout" in roadmap, "missing complete pipeline layout")
    require("038 DOF -> 031 TAA -> 038 motion blur -> 037 bloom" in roadmap, "pipeline DOF/TAA/motion-blur/bloom order is not explicit")
    layout = roadmap.split("## Complete Rendering Pipeline Layout", 1)[-1].split("## Parallel Development Tracks", 1)[0]
    composition, transfer = layout.find("| UI composition |"), layout.find("| Output transfer/native packing")
    require(composition >= 0 and transfer > composition, "UI must precede sole output transfer in pipeline layout")
    version = re.search(r"\*\*Version\*\*: ([0-9.]+)", roadmap)
    updated = re.search(r"\*\*Last Updated\*\*: (\d{4}-\d{2}-\d{2})", roadmap)
    changes = re.findall(r"^\| (\d{4}-\d{2}-\d{2}) \| ([0-9.]+) \|", roadmap, re.M)
    require(bool(version and updated and changes) and (updated[1], version[1]) == changes[0]
            and version[1] == index.get("roadmapVersion"), "roadmap version/date/index/change-log mismatch")

    task_pattern = r"^- \[[ xX]\] T(\d{3})\b"
    roadmap_tasks = sequence("roadmap tasks", docs.get("tasks.md", ""), task_pattern)
    feature_tasks = sequence("Feature 029 tasks", feature["tasks.md"], task_pattern, 118)
    for label, text, prefix, maximum in [("roadmap FR", docs.get("spec.md", ""), "FR", 33),
            ("roadmap SC", docs.get("spec.md", ""), "SC", 8), ("Feature 029 FR", feature["spec.md"], "FR", 46),
            ("Feature 029 SC", feature["spec.md"], "SC", 16)]:
        sequence(label, text, rf"^- \*\*{prefix}-(\d{{3}})\*\*:", maximum)
    for group, maximum in ((docs, roadmap_tasks), (feature, feature_tasks)):
        for name, text in group.items():
            for ref in set(re.findall(r"\bT(\d{3})\b", text)):
                require(1 <= int(ref) <= maximum, f"{name}: unknown task reference T{ref}")
    require("EDRMetadata=nil" in feature["spec.md"] and "BGR10A2Unorm" in feature["spec.md"],
            "Feature 029 Apple PQ/EDR governance is incomplete")

    active_roadmap = roadmap[:start] + roadmap[end:] if start >= 0 and end > start else roadmap
    active = {"doc/roadmap.md": active_roadmap.split("## Change Log")[0], "AGENTS.md": agents}
    for name, text in docs.items():
        if name in ("migration-3.0.md", "migration-3.0.1.md"):
            continue
        if name == "spec.md":
            text = re.sub(r"## Clarifications[\s\S]*?(?=## Assumptions)", "", text)
        if name == "tasks.md":
            text = text[text.find("## Phase 13:"):]
        active[name] = text
    aliases = {31: r"Meshlet", 32: r"GPU[- ]Driven", 33: r"Streaming", 34: r"(?:DirectX 12|DX12)",
               35: r"OpenGL", 36: r"GLES", 37: r"(?:Ray Tracing.*Foundation|RT Foundation)",
               38: r"Ray[- ]Traced (?:Renderer )?Effects", 39: r"Screen[- ]Space GI",
               40: r"SDF.*Surface Cache", 41: r"Hybrid GI"}
    previous_names = {30: "Anti-Aliasing & Temporal Reconstruction", 31: "Raster Shadow Maps & Cascades", 32: "Screen-Space Shadows & Shadow Filtering", 33: "Sky Atmosphere & Environment Lighting", 34: "Height Fog & Volumetric Fog", 35: "Volumetric Clouds", 36: "Exposure, Bloom & Color Grading", 37: "Depth of Field & Motion Blur", 38: "Virtual Shadow Maps", 39: "Screen-Space Ambient Occlusion & Reflections", 40: "Frame Profiling & Render Diagnostics", 41: "Complete Rendering Pipeline Integration & Quality Baseline", 42: "Meshlet Derived Data", 43: "GPU-Driven Visibility & LOD", 44: "Streaming & Residency", 45: "Screen-Space GI & Temporal", 46: "Ray Tracing & Vulkan Backend Foundation", 47: "Ray-Traced Renderer Effects", 48: "SDF & Surface Cache Assets", 49: "Hybrid GI Integration", 50: "DirectX 12 Backend", 51: "OpenGL Backend", 52: "GLES Backend"}
    for name, text in active.items():
        for old, previous_name in previous_names.items():
            pattern = rf"(?:Feature|Phase) {old:03d}\s*(?:[-—:]\s*)?(?:(?:Renderer|Asset|Backend|RHI|Application):\s*)?{re.escape(previous_name)}"
            require(not re.search(pattern, text, re.I), f"{name}: stale active 3.0.1 phase reference {old:03d}")
        for number, alias in aliases.items():
            pattern = rf"(?:Feature|Phase) {number:03d}\s*(?:[-—:]\s*)?(?:(?:Renderer|Asset|Backend|RHI):\s*)?{alias}"
            require(not re.search(pattern, text, re.I), f"{name}: stale active phase reference {number:03d}")
        require(not re.search(r"(?:Feature|Phase) 031\s*(?:[-—:]\s*)?(?:Renderer:\s*)?(?:Frame |Full )?Profiling", text, re.I),
                f"{name}: stale active profiling reference 031")
        require(not re.search(r"003(?:-| through )(?:041|052)|(?:39|50) runtime phases", text),
                f"{name}: stale active runtime range")
    checks.update({"roadmapTaskCount": roadmap_tasks, "feature029TaskCount": feature_tasks,
                   "feature029RequirementCount": 46, "feature029SuccessCriterionCount": 16,
                   "roadmapRequirementCount": 33, "roadmapSuccessCriterionCount": 8,
                   "activeReferenceFileCount": len(active), "completedPhaseDetailSha256": COMPLETED_DETAIL_SHA256})
    return result(checks, findings)


def main() -> int:
    report = scan()
    print(json.dumps(report, sort_keys=True, separators=(",", ":")))
    return 0 if not report["findings"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
