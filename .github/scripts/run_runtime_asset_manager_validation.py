#!/usr/bin/env python3
"""Run normalized Feature 026 focused and regression validation."""

from __future__ import annotations

import argparse
import json
import pathlib
import shutil
import subprocess
import sys
from dataclasses import dataclass
from typing import Sequence


@dataclass(frozen=True)
class CommandResult:
    name: str
    command: tuple[str, ...]
    returncode: int


def suite_command(tests: pathlib.Path, *suites: str) -> list[str]:
    command = [str(tests)]
    for suite in suites:
        command.extend(("--suite", suite))
    return command


def run_command(
    name: str,
    command: Sequence[str],
    root: pathlib.Path,
    timeout: int,
) -> CommandResult:
    try:
        completed = subprocess.run(
            list(command), cwd=root, check=False, timeout=timeout,
        )
        return CommandResult(name, tuple(str(value) for value in command),
                             completed.returncode)
    except subprocess.TimeoutExpired:
        return CommandResult(name, tuple(str(value) for value in command), 124)


def write_summary(
    path: pathlib.Path,
    profile: str,
    results: Sequence[CommandResult],
    prepared: bool = False,
) -> None:
    payload = {
        "schema": "stoner.runtime-asset-manager-validation",
        "schemaVersion": 1,
        "feature": "026-runtime-asset-manager",
        "profile": profile,
        "prepared": prepared,
        "commands": [
            {"name": result.name, "exitCode": result.returncode}
            for result in results
        ],
        "passed": bool(results) and all(
            result.returncode == 0 for result in results
        ),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def parse_args(values: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--tests", type=pathlib.Path)
    parser.add_argument("--tests-debug", type=pathlib.Path)
    parser.add_argument("--tests-release", type=pathlib.Path)
    parser.add_argument("--profile", type=pathlib.Path)
    parser.add_argument("--work", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--repetitions", type=int, default=20)
    parser.add_argument("--timeout-seconds", type=int, default=1200)
    parser.add_argument("--full-regression", action="store_true")
    parser.add_argument("--skip-benchmark", action="store_true")
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args(values)
    if args.repetitions != 20:
        parser.error("--repetitions must be exactly 20")
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")
    if not args.prepare_only and not (
        args.tests or args.tests_debug or args.tests_release
    ):
        parser.error("at least one test executable is required")
    return args


def resolve(root: pathlib.Path, path: pathlib.Path | None) -> pathlib.Path | None:
    if path is None:
        return None
    return path if path.is_absolute() else (root / path).resolve()


def main(values: Sequence[str] | None = None) -> int:
    args = parse_args(values)
    root = args.root.resolve()
    output = resolve(root, args.output)
    work = resolve(root, args.work)
    assert output is not None and work is not None
    if args.prepare_only:
        write_summary(output, "pending", [], prepared=True)
        return 0

    debug = resolve(root, args.tests_debug)
    release = resolve(root, args.tests_release or args.tests)
    profile_name = "full-regression" if args.full_regression else (
        "debug" if debug and not release else
        "release" if release and not debug else "debug-release"
    )
    for path in (debug, release):
        if path is not None and not path.is_file():
            raise FileNotFoundError(path)
    if args.profile is not None:
        profile = resolve(root, args.profile)
        if profile is None or not profile.is_file():
            raise FileNotFoundError(profile)

    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)
    results: list[CommandResult] = []

    verifiers = [
        ("runtime-contract", [sys.executable,
            "Tests/verify_runtime_asset_manager.py", "--root", str(root)]),
        ("asset-boundary", [sys.executable,
            "Tests/verify_asset_layer.py"]),
        ("architecture", [sys.executable,
            "Tests/verify_architecture.py"]),
        ("contract-unit", [sys.executable, "-m", "unittest",
            "Tests/test_verify_runtime_asset_manager.py"]),
        ("runner-unit", [sys.executable,
            ".github/scripts/test_run_runtime_asset_manager_validation.py"]),
    ]
    for name, command in verifiers:
        results.append(run_command(name, command, root, args.timeout_seconds))

    if debug is not None:
        results.append(run_command("debug-focused", suite_command(
            debug, "asset-manager", "asset-manager-cooked"),
            root, args.timeout_seconds))
        if args.full_regression:
            results.append(run_command(
                "debug-regression", [str(debug)], root, args.timeout_seconds
            ))

    if release is not None:
        results.append(run_command("release-focused", suite_command(
            release, "asset-manager", "asset-manager-cooked"),
            root, args.timeout_seconds))
        if not args.skip_benchmark:
            benchmark = work / "performance.txt"
            results.append(run_command("release-benchmark", [
                str(release), "--suite", "asset-manager-benchmark",
                "--asset-manager-benchmark-profile", "ci",
                "--asset-manager-benchmark-report", str(benchmark),
            ], root, args.timeout_seconds))
        if args.full_regression:
            results.append(run_command(
                "release-regression", [str(release)], root,
                args.timeout_seconds,
            ))

    write_summary(output, profile_name, results)
    return 0 if all(result.returncode == 0 for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
