"""Extract specification requirements and seed traceability records."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any

from reviewlib import write_csv


FEATURE_RANGE = range(3, 20)
REQUIREMENT_PATTERN = re.compile(
    r"^\s*[-*]\s+\*\*(FR|SC)-(\d{3}[a-z]?)\*\*:\s*(.+?)\s*$"
)
TRACE_FIELDS = [
    "trace_id",
    "feature",
    "kind",
    "requirement_id",
    "requirement",
    "spec_path",
    "api",
    "implementation",
    "tests",
    "ci_evidence",
    "classification",
    "notes",
]


def feature_specs(repo: Path) -> list[Path]:
    specs = []
    for feature in FEATURE_RANGE:
        matches = sorted((repo / "specs").glob(f"{feature:03d}-*/spec.md"))
        specs.extend(matches)
    return specs


def extract(repo: Path) -> list[dict[str, Any]]:
    records: list[dict[str, Any]] = []
    for path in feature_specs(repo):
        feature = path.parent.name[:3]
        for line_number, line in enumerate(
            path.read_text(encoding="utf-8").splitlines(), start=1
        ):
            match = REQUIREMENT_PATTERN.match(line)
            if not match:
                continue
            kind, number, requirement = match.groups()
            requirement_id = f"{kind}-{number}"
            records.append(
                {
                    "trace_id": f"{feature}-{requirement_id}",
                    "feature": feature,
                    "kind": kind,
                    "requirement_id": requirement_id,
                    "requirement": requirement,
                    "spec_path": f"{path.relative_to(repo)}:{line_number}",
                    "api": "",
                    "implementation": "",
                    "tests": "",
                    "ci_evidence": "",
                    "classification": "Unclassified",
                    "notes": "",
                }
            )
    return records


def write_seed(repo: Path, output: Path) -> int:
    records = extract(repo)
    write_csv(output, TRACE_FIELDS, records)
    return len(records)
