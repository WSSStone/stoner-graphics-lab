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
    current = status(repo)
    initialized = bool(
        current.get("available")
        and isinstance(current.get("raw"), dict)
        and current["raw"].get("initialized")
    )
    command = "index" if initialized else "init"
    result = run(["codegraph", command, str(repo)], cwd=repo, check=False, timeout=1800)
    if result.returncode != 0:
        raise ReviewError(result.stderr.strip() or result.stdout.strip())
    return status(repo)


def cpp_inventory(repo: Path) -> list[str]:
    roots = [repo / "Source", repo / "Tests", repo / "Demo"]
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


def indexed_files(repo: Path) -> list[dict[str, Any]]:
    result = run(
        ["codegraph", "files", "--path", str(repo), "--format", "flat", "--json"],
        cwd=repo,
        check=False,
    )
    if result.returncode != 0:
        raise ReviewError(result.stderr.strip() or result.stdout.strip())
    try:
        records = json.loads(result.stdout)
    except json.JSONDecodeError as exc:
        raise ReviewError(f"invalid CodeGraph file inventory: {exc}") from exc
    if not isinstance(records, list):
        raise ReviewError("CodeGraph file inventory must be a JSON list")
    return records


def cpp_coverage(repo: Path) -> dict[str, Any]:
    expected = set(cpp_inventory(repo))
    indexed = {
        record["path"]
        for record in indexed_files(repo)
        if record.get("language") in {"c", "cpp"}
        and Path(record.get("path", "")).suffix in {".h", ".hpp", ".cpp", ".cc"}
        and record.get("path", "").split("/", maxsplit=1)[0] in {"Source", "Tests", "Demo"}
    }
    missing = sorted(expected - indexed)
    unexpected = sorted(indexed - expected)
    return {
        "expected_cpp_files": len(expected),
        "indexed_cpp_files": len(indexed),
        "coverage_percent": round(100.0 * len(expected & indexed) / len(expected), 2)
        if expected
        else 100.0,
        "missing": missing,
        "unexpected": unexpected,
    }
