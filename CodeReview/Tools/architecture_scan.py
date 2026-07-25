"""Conservative include-boundary scanner for project-owned C++ files."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any


INCLUDE_PATTERN = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]')
LAYERS = ("Core", "Asset", "RHI", "Backend", "Renderer", "Application")
ALLOWED = {
    "Core": {"Core"},
    "Asset": {"Asset", "Core"},
    "RHI": {"RHI", "Core"},
    "Backend": {"Backend", "RHI", "Core"},
    "Renderer": {"Renderer", "Asset", "RHI", "Core"},
    "Application": {"Application", "Renderer", "Asset", "Core"},
}


def source_layer(path: Path) -> str | None:
    parts = path.parts
    if "Source" not in parts:
        return None
    index = parts.index("Source")
    return parts[index + 1] if index + 1 < len(parts) else None


def include_layer(include: str) -> str | None:
    first = include.replace("\\", "/").split("/", maxsplit=1)[0]
    return first if first in LAYERS else None


def scan(repo: Path) -> list[dict[str, Any]]:
    violations: list[dict[str, Any]] = []
    for path in sorted((repo / "Source").rglob("*")):
        if path.suffix not in {".h", ".hpp", ".cpp", ".cc"}:
            continue
        owner = source_layer(path.relative_to(repo))
        if owner not in ALLOWED:
            continue
        for line_number, line in enumerate(
            path.read_text(encoding="utf-8", errors="replace").splitlines(), start=1
        ):
            match = INCLUDE_PATTERN.match(line)
            dependency = include_layer(match.group(1)) if match else None
            if dependency and dependency not in ALLOWED[owner]:
                violations.append(
                    {
                        "file": str(path.relative_to(repo)),
                        "line": line_number,
                        "owner": owner,
                        "dependency": dependency,
                        "include": match.group(1),
                    }
                )
    return violations
