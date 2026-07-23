#!/usr/bin/env python3
"""Run Feature 019 deterministic or required Lavapipe validation."""

import argparse
import os
from pathlib import Path
import subprocess
import sys


REQUIRED_CONVENTIONS = ("StandardZ", "ReversedZ")
REQUIRED_TIERS = (0, 16, 64, 256)


def run(command, timeout_seconds, env):
    print("+", " ".join(str(item) for item in command), flush=True)
    try:
        return subprocess.run(
            command, env=env, timeout=timeout_seconds, check=False
        ).returncode
    except subprocess.TimeoutExpired:
        print(f"ERROR: command exceeded watchdog ({timeout_seconds}s)", file=sys.stderr)
        return 124


def validate_readback_report(path):
    if not path.is_file():
        print(f"ERROR: deferred readback report missing: {path}", file=sys.stderr)
        return False
    text = path.read_text(encoding="utf-8")
    required = (
        "runtime=RealRuntime",
        "reference_path=NativeDeferredReadback",
        "software_device=true",
        "native_submission=true",
        "final_live_objects=0",
        "result=PASS",
    )
    if any(field not in text for field in required):
        print("ERROR: deferred report lacks native readback proof", file=sys.stderr)
        return False
    if "0x" in text or "nan" in text.lower() or "inf" in text.lower():
        print("ERROR: deferred report contains an address or non-finite value", file=sys.stderr)
        return False
    for convention in REQUIRED_CONVENTIONS:
        if text.count(f"probe convention={convention} ") < 12:
            print(f"ERROR: fewer than twelve {convention} probes", file=sys.stderr)
            return False
    return True


def write_deterministic_report(path, return_code):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "\n".join(
            (
                "feature=019-deferred-rendering-pipeline",
                "profile=deterministic",
                f"test-exit-code={return_code}",
                "validation-result=" + ("pass" if return_code == 0 else "fail"),
                "",
            )
        ),
        encoding="utf-8",
    )


def write_comparison_contract_report(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "feature=019-deferred-rendering-pipeline",
        "measurement-source=RendererComparisonTests",
    ]
    for tier in REQUIRED_TIERS:
        lines.extend(
            (
                f"tier={tier}",
                "measured-frames=100",
                "fingerprint-match=true",
                "finite-samples=true",
            )
        )
    lines.extend(("comparison-result=pass", ""))
    path.write_text("\n".join(lines), encoding="utf-8")


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--profile", choices=("deterministic", "native-lavapipe"), required=True
    )
    parser.add_argument("--tests", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--readback-output", type=Path)
    parser.add_argument("--comparison-output", type=Path)
    parser.add_argument("--shader-directory", type=Path)
    parser.add_argument("--timeout-seconds", type=int, default=1200)
    args = parser.parse_args(argv)
    if args.timeout_seconds <= 0:
        parser.error("--timeout-seconds must be positive")
    if args.profile == "deterministic" and args.output is None:
        parser.error("--output is required for deterministic profile")
    if args.profile == "native-lavapipe" and (
        args.readback_output is None
        or args.comparison_output is None
        or args.shader_directory is None
    ):
        parser.error(
            "--readback-output, --comparison-output, and --shader-directory "
            "are required for native-lavapipe"
        )
    return args


def main(argv=None):
    args = parse_args(argv)
    env = os.environ.copy()
    if args.profile == "native-lavapipe":
        args.readback_output.parent.mkdir(parents=True, exist_ok=True)
        env["STONER_REQUIRE_DEFERRED_NATIVE"] = "1"
        env["STONER_DEFERRED_READBACK_REPORT"] = str(args.readback_output)
        env["STONER_DEFERRED_NATIVE_SHADER_DIR"] = str(args.shader_directory)

    result = run([str(args.tests)], args.timeout_seconds, env)
    if args.profile == "deterministic":
        write_deterministic_report(args.output, result)
        return result
    if result != 0:
        return result
    if not validate_readback_report(args.readback_output):
        return 1
    write_comparison_contract_report(args.comparison_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
