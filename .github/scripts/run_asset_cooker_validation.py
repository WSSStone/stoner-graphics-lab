#!/usr/bin/env python3
"""Run the clean-checkout Feature 025 cross-platform validation profile."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from typing import Sequence


@dataclass(frozen=True)
class CommandResult:
    seconds: float
    stdout: str
    stderr: str


def run(command: Sequence[str], root: pathlib.Path, timeout: int) -> CommandResult:
    started = time.monotonic()
    result = subprocess.run(
        list(command), cwd=root, check=False, capture_output=True, text=True,
        timeout=timeout,
    )
    elapsed = time.monotonic() - started
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    return CommandResult(elapsed, result.stdout, result.stderr)


def load_normalized_report(path: pathlib.Path, expected: str = "success") -> dict:
    value = json.loads(path.read_bytes())
    if value.get("schema") != "stoner.asset-cook-report" or \
            value.get("result") != expected or "telemetry" in value:
        raise ValueError(f"invalid normalized report: {path}")
    return value


def digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def compare_reports(paths: Sequence[pathlib.Path]) -> str:
    if not paths:
        raise ValueError("no reports to compare")
    expected = paths[0].read_bytes()
    for path in paths[1:]:
        if path.read_bytes() != expected:
            raise ValueError(f"normalized reports differ: {paths[0]} vs {path}")
    return hashlib.sha256(expected).hexdigest()


def suite_command(tests: pathlib.Path, *suites: str) -> list[str]:
    command = [str(tests)]
    for suite in suites:
        command.extend(["--suite", suite])
    return command


def command_line(
    cooker: pathlib.Path,
    command: str,
    source: pathlib.Path,
    output: pathlib.Path,
    ddc: pathlib.Path,
    profile: pathlib.Path,
    report: pathlib.Path,
    workers: int,
    clean: bool = False,
) -> list[str]:
    value = [
        str(cooker), command,
        "--source-root", str(source), "--cook-all",
        "--target-profile", str(profile),
        "--output", str(output), "--ddc", str(ddc),
        "--workers", str(workers), "--normalized-report",
        "--report", str(report),
    ]
    if command == "cook":
        value.extend(["--lease-timeout-ms", "30000"])
        if clean:
            value.append("--clean")
    return value


def parse_args(values: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path.cwd())
    parser.add_argument("--cooker", type=pathlib.Path, required=True)
    parser.add_argument("--tests", type=pathlib.Path, required=True)
    parser.add_argument(
        "--profile", type=pathlib.Path,
        default=pathlib.Path("Config/AssetCooker/Profiles/Mac-Vulkan.json"),
    )
    parser.add_argument(
        "--source", type=pathlib.Path,
        default=pathlib.Path("Tests/Fixtures/AssetCooker/Representative"),
    )
    parser.add_argument("--work", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--determinism-runs", type=int, default=2)
    parser.add_argument("--timeout-seconds", type=int, default=1200)
    parser.add_argument("--skip-suites", action="store_true")
    parser.add_argument("--skip-benchmark", action="store_true")
    args = parser.parse_args(values)
    if args.determinism_runs < 2 or args.determinism_runs > 20:
        parser.error("--determinism-runs must be in [2, 20]")
    return args


def main(values: Sequence[str] | None = None) -> int:
    args = parse_args(values)
    root = args.root.resolve()
    cooker = (root / args.cooker).resolve() if not args.cooker.is_absolute() else args.cooker
    tests = (root / args.tests).resolve() if not args.tests.is_absolute() else args.tests
    profile = (root / args.profile).resolve() if not args.profile.is_absolute() else args.profile
    source_fixture = (root / args.source).resolve() if not args.source.is_absolute() else args.source
    work = (root / args.work).resolve() if not args.work.is_absolute() else args.work
    output_summary = (root / args.output).resolve() if not args.output.is_absolute() else args.output
    for path in (cooker, tests, profile):
        if not path.is_file():
            raise FileNotFoundError(path)
    if not source_fixture.is_dir():
        raise FileNotFoundError(source_fixture)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)

    reports: dict[str, list[pathlib.Path]] = {
        name: [] for name in ("plan", "clean", "incremental", "validate", "standalone")
    }
    timings: dict[str, list[float]] = {name: [] for name in reports}
    generations: list[str] = []
    for index in range(args.determinism_runs):
        run_root = work / f"run-{index:02d}"
        source = run_root / "Source"
        output = run_root / "Cooked"
        ddc = run_root / "DDC"
        report_root = run_root / "Reports"
        shutil.copytree(source_fixture, source)
        report_root.mkdir(parents=True)
        workers = 1 if index % 2 == 0 else 8

        plan_report = report_root / "plan.json"
        result = run(command_line(
            cooker, "plan", source, output, ddc, profile, plan_report, workers,
        ), root, args.timeout_seconds)
        timings["plan"].append(result.seconds)
        reports["plan"].append(plan_report)
        load_normalized_report(plan_report)
        if output.exists() or ddc.exists() or pathlib.Path(str(output) + ".scratch").exists():
            raise RuntimeError("plan mutated output, DDC, or scratch")

        clean_report = report_root / "clean.json"
        result = run(command_line(
            cooker, "cook", source, output, ddc, profile, clean_report,
            workers, clean=True,
        ), root, args.timeout_seconds)
        timings["clean"].append(result.seconds)
        reports["clean"].append(clean_report)
        clean = load_normalized_report(clean_report)
        generations.append(clean["generationId"])

        incremental_report = report_root / "incremental.json"
        try:
            result = run(command_line(
                cooker, "cook", source, output, ddc, profile,
                incremental_report, 8 if workers == 1 else 1,
            ), root, args.timeout_seconds)
        except RuntimeError as failure:
            diagnostic = subprocess.run(
                [str(cooker), "validate", "--output", str(output),
                 "--strict-files"],
                cwd=root, check=False, capture_output=True, text=True,
                timeout=args.timeout_seconds,
            )
            raise RuntimeError(
                f"{failure}\npost-failure published validation "
                f"exit={diagnostic.returncode}\n"
                f"stdout:\n{diagnostic.stdout}\n"
                f"stderr:\n{diagnostic.stderr}"
            ) from failure
        timings["incremental"].append(result.seconds)
        reports["incremental"].append(incremental_report)
        incremental = load_normalized_report(incremental_report)
        if incremental["summary"]["reused"] != incremental["summary"]["reachable"]:
            raise RuntimeError("unchanged incremental cook did not reuse every asset")

        validate_report = report_root / "validate.json"
        result = run([
            str(cooker), "validate", "--output", str(output), "--strict-files",
            "--normalized-report", "--report", str(validate_report),
        ], root, args.timeout_seconds)
        timings["validate"].append(result.seconds)
        reports["validate"].append(validate_report)
        load_normalized_report(validate_report)

        hidden_source = run_root / "Source.hidden"
        hidden_ddc = run_root / "DDC.hidden"
        source.rename(hidden_source)
        ddc.rename(hidden_ddc)
        standalone_report = report_root / "standalone.json"
        result = run([
            str(cooker), "validate", "--output", str(output), "--strict-files",
            "--normalized-report", "--report", str(standalone_report),
        ], root, args.timeout_seconds)
        timings["standalone"].append(result.seconds)
        reports["standalone"].append(standalone_report)
        load_normalized_report(standalone_report)

    report_digests = {
        name: compare_reports(paths) for name, paths in reports.items()
    }
    if len(set(generations)) != 1:
        raise RuntimeError("clean generation identity differs across runs")

    thresholds = {
        "plan": 8.0,
        "clean": 240.0,
        "incremental": 40.0,
        "validate": 40.0,
        "standalone": 40.0,
    }
    for name, samples in timings.items():
        if max(samples) > thresholds[name]:
            raise RuntimeError(
                f"CI 4x smoke limit exceeded: {name} {max(samples):.6f}s"
            )

    suite_results: dict[str, CommandResult] = {}
    if not args.skip_suites:
        suite_groups = {
            "operator": (
                "asset-cooker-cli", "asset-cooker-report",
                "asset-cooker-workflow",
            ),
            "ddc-corruption-15": ("asset-cooker-ddc",),
            "published-corruption-30": ("asset-cooker-published-validation",),
            "concurrency": (
                "asset-cooker-concurrency",
                "asset-cooker-publication-concurrency",
                "core-file-lease",
            ),
        }
        suite_log_root = work / "suite-logs"
        suite_log_root.mkdir(parents=True)
        for name, suites in suite_groups.items():
            result = run(suite_command(tests, *suites), root, args.timeout_seconds)
            suite_results[name] = result
            (suite_log_root / f"{name}.txt").write_text(
                result.stdout + result.stderr, encoding="utf-8"
            )

    benchmark_result: CommandResult | None = None
    benchmark_report = work / "benchmark.txt"
    if not args.skip_benchmark:
        benchmark_result = run([
            str(tests), "--suite", "asset-cooker-benchmark",
            "--asset-cooker-benchmark-profile", "ci",
            "--asset-cooker-benchmark-report", str(benchmark_report),
        ], root, args.timeout_seconds)

    for verifier in (
        "Tests/verify_asset_cooker_contracts.py",
        "Tests/verify_asset_cooker_fixtures.py",
        "Tests/verify_architecture.py",
    ):
        run([sys.executable, verifier], root, args.timeout_seconds)

    corruption_counts = {
        "cache": len(list((
            root / "Tests/Fixtures/AssetCooker/CorruptCache"
        ).glob("*.case.json"))),
        "published": len(list((
            root / "Tests/Fixtures/AssetCooker/CorruptPublished"
        ).glob("*.case.json"))),
    }
    if corruption_counts != {"cache": 15, "published": 30}:
        raise RuntimeError(f"corruption corpus count mismatch: {corruption_counts}")

    summary = {
        "schema": "stoner.asset-cooker-validation-summary",
        "schemaVersion": 1,
        "runs": args.determinism_runs,
        "workers": [1, 8],
        "generationId": generations[0],
        "reportDigests": report_digests,
        "maximumSeconds": {
            name: max(samples) for name, samples in timings.items()
        },
        "thresholdSeconds": thresholds,
        "corruptCacheCases": corruption_counts["cache"],
        "corruptPublishedCases": corruption_counts["published"],
        "suiteSeconds": {
            name: result.seconds for name, result in suite_results.items()
        } if suite_results else None,
        "benchmarkSeconds": benchmark_result.seconds if benchmark_result else None,
        "benchmarkReportSha256": digest(benchmark_report)
            if benchmark_report.is_file() else None,
        "passed": True,
    }
    output_summary.parent.mkdir(parents=True, exist_ok=True)
    output_summary.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8",
    )
    print(json.dumps(summary, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
