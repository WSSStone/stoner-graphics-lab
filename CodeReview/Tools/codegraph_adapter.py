"""Versioned, bounded adapter around the CodeGraph CLI."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any

from reviewlib import ReviewError, run


def version(repo: Path) -> str | None:
    result = run(["codegraph", "--version"], cwd=repo, check=False)
    return result.stdout.strip() if result.returncode == 0 else None


def status(repo: Path) -> dict[str, Any]:
    result = run(["codegraph", "status", "--json", str(repo)], cwd=repo, check=False)
    if result.returncode == 0:
        try:
            return {"available": True, "raw": json.loads(result.stdout)}
        except json.JSONDecodeError:
            pass
    fallback = run(["codegraph", "status", str(repo)], cwd=repo, check=False)
    return {
        "available": fallback.returncode == 0,
        "raw": fallback.stdout.strip() or fallback.stderr.strip(),
    }


def rebuild(repo: Path) -> dict[str, Any]:
    result = run(["codegraph", "index", str(repo)], cwd=repo, check=False, timeout=1800)
    if result.returncode != 0:
        raise ReviewError(result.stderr.strip() or result.stdout.strip())
    return status(repo)


def cpp_inventory(repo: Path) -> list[str]:
    roots = [repo / "Source", repo / "Tests"]
    files: list[str] = []
    for root in roots:
        if not root.exists():
            continue
        files.extend(
            str(path.relative_to(repo))
            for path in root.rglob("*")
            if path.suffix in {".h", ".hpp", ".cpp", ".cc"}
        )
    return sorted(files)
