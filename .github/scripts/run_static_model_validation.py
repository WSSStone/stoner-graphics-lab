#!/usr/bin/env python3
"""Run Feature 024 deterministic or required native realization validation."""

import argparse
import os
from pathlib import Path
import subprocess
import sys


def run(command, timeout_seconds, env):
    print("+", " ".join(str(item) for item in command), flush=True)
    try:
        return subprocess.run(
            command, env=env, timeout=timeout_seconds, check=False
        ).returncode
    except subprocess.TimeoutExpired:
        print(f"ERROR: command exceeded watchdog ({timeout_seconds}s)", file=sys.stderr)
        return 124


def write_report(path, profile, results):
    path.parent.mkdir(parents=True, exist_ok=True)
    passed = all(code == 0 for code in results.values())
    lines = [
        "feature=024-static-mesh-model",
        f"profile={profile}",
    ]
    lines.extend(f"{name}-exit-code={code}" for name, code in results.items())
    lines.extend(
        (
            "renderer-buffer-uploads=required",
            "indexed-clockwise-readback=required",
            "non-symmetric-matrix-readback=required" if profile == "native" else
            "non-symmetric-matrix-readback=not-run",
            "validation-result=" + ("pass" if passed else "fail"),
            "",
        )
    )
    path.write_text("\n".join(lines), encoding="utf-8")


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--profile", choices=("deterministic", "native"), required=True)
    parser.add_argument("--tests", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=int, default=1200)
    args = parser.parse_args(argv)
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")
    return args


def main(argv=None):
    args = parse_args(argv)
    env = os.environ.copy()
    suites = ["renderer-static-mesh"]
    if args.profile == "native":
        env["STONER_REQUIRE_STATIC_MESH_NATIVE"] = "1"
        env["STONER_REQUIRE_DEFERRED_NATIVE"] = "1"
        suites.extend(("vulkan-native", "deferred-native"))

    results = {}
    for suite in suites:
        code = run(
            [str(args.tests), "--suite", suite],
            args.timeout_seconds,
            env,
        )
        results[suite] = code
        if code != 0:
            break
    write_report(args.output, args.profile, results)
    return 0 if all(code == 0 for code in results.values()) else next(
        code for code in results.values() if code != 0
    )


if __name__ == "__main__":
    raise SystemExit(main())
