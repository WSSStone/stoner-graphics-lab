"""Shared state, filesystem, and subprocess helpers for CR tools."""

from __future__ import annotations

import csv
import json
import os
import subprocess
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Iterable, Sequence


SCHEMA_VERSION = 1
RUNS_RELATIVE = Path("CodeReview") / "Runs"
FINDING_STATES = {
    "Open",
    "Triaged",
    "Accepted",
    "Deferred",
    "Rejected",
    "Fixed",
    "Verified",
}
SEVERITIES = {"S0", "S1", "S2", "S3"}


class ReviewError(RuntimeError):
    """Expected user-facing review protocol error."""


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def find_repo_root(start: Path | None = None) -> Path:
    current = (start or Path.cwd()).resolve()
    result = run(["git", "rev-parse", "--show-toplevel"], cwd=current, check=False)
    if result.returncode != 0:
        raise ReviewError(f"not inside a Git repository: {current}")
    return Path(result.stdout.strip()).resolve()


def run(
    args: Sequence[str],
    *,
    cwd: Path,
    check: bool = True,
    timeout: int = 300,
) -> subprocess.CompletedProcess[str]:
    if not args or any(not isinstance(item, str) for item in args):
        raise ReviewError("subprocess arguments must be a non-empty string list")
    result = subprocess.run(
        list(args),
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
        shell=False,
    )
    if check and result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise ReviewError(f"command failed ({result.returncode}): {' '.join(args)}\n{detail}")
    return result


def read_json(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise ReviewError(f"missing required state file: {path}") from exc
    except json.JSONDecodeError as exc:
        raise ReviewError(f"invalid JSON in {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise ReviewError(f"expected a JSON object in {path}")
    return value


def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    encoded = json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False) + "\n"
    atomic_write(path, encoded)


def atomic_write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="") as stream:
            stream.write(content)
        os.replace(temporary, path)
    except Exception:
        Path(temporary).unlink(missing_ok=True)
        raise


def locate_run(repo: Path, run_id: str | None) -> Path:
    root = repo / RUNS_RELATIVE
    if run_id:
        matches = sorted(root.glob(f"{run_id}-*"))
        if len(matches) != 1:
            raise ReviewError(f"expected exactly one run matching {run_id}, found {len(matches)}")
        return matches[0]
    candidates = sorted(path for path in root.glob("CR-*") if (path / "state.json").is_file())
    if len(candidates) != 1:
        raise ReviewError("pass --id because zero or multiple review runs are present")
    return candidates[0]


def load_run(repo: Path, run_id: str | None) -> tuple[Path, dict[str, Any]]:
    run_dir = locate_run(repo, run_id)
    return run_dir, read_json(run_dir / "state.json")


def git_snapshot(repo: Path) -> dict[str, Any]:
    status = run(["git", "status", "--porcelain=v1"], cwd=repo).stdout.splitlines()
    return {
        "head": run(["git", "rev-parse", "HEAD"], cwd=repo).stdout.strip(),
        "branch": run(["git", "branch", "--show-current"], cwd=repo).stdout.strip(),
        "dirty": bool(status),
        "changes": status,
    }


def append_event(state: dict[str, Any], kind: str, detail: str) -> None:
    state.setdefault("events", []).append(
        {"at": utc_now(), "kind": kind, "detail": detail}
    )
    state["updated_at"] = utc_now()


def read_csv(path: Path) -> list[dict[str, str]]:
    if not path.exists():
        return []
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream))


def write_csv(path: Path, fieldnames: Iterable[str], rows: Iterable[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    descriptor, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(fieldnames))
            writer.writeheader()
            writer.writerows(rows)
        os.replace(temporary, path)
    except Exception:
        Path(temporary).unlink(missing_ok=True)
        raise
