#!/usr/bin/env python3
"""Run a pinned Khronos ktx validator without invoking a shell."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys
from typing import Mapping, Sequence


ALLOWED_CUSTOM_KEYS = frozenset(
    {
        "stoner.alphaMode",
        "stoner.assetId",
        "stoner.channelCount",
        "stoner.contentDigest",
        "stoner.cookRevision",
        "stoner.mipPolicy",
        "stoner.portableProfile",
        "stoner.semantic",
        "stoner.sourceDigest",
    }
)


def run_command(
    command: Sequence[str], timeout_seconds: float
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        list(command),
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout_seconds,
        shell=False,
    )


def tool_version(executable: str, timeout_seconds: float) -> str:
    result = run_command([executable, "--version"], timeout_seconds)
    if result.returncode != 0:
        raise RuntimeError("version command failed")
    return (result.stdout + result.stderr).strip()


def discover(inputs: Sequence[pathlib.Path]) -> list[pathlib.Path]:
    artifacts: set[pathlib.Path] = set()
    for path in inputs:
        if path.is_file() and path.suffix.lower() == ".ktx2":
            artifacts.add(path.resolve())
        elif path.is_dir():
            artifacts.update(
                candidate.resolve()
                for candidate in path.rglob("*")
                if candidate.is_file()
                and candidate.suffix.lower() == ".ktx2"
            )
    return sorted(artifacts, key=lambda path: path.as_posix())


def build_artifact_labels(
    inputs: Sequence[pathlib.Path],
    artifacts: Sequence[pathlib.Path],
) -> dict[pathlib.Path, str]:
    resolved_inputs = [path.resolve() for path in inputs]
    labels: dict[pathlib.Path, str] = {}
    for artifact in artifacts:
        resolved_artifact = artifact.resolve()
        candidates: list[str] = []
        for index, root in enumerate(resolved_inputs):
            if root.is_file() and root == resolved_artifact:
                candidates.append(
                    f"input-{index:02d}/{resolved_artifact.name}"
                )
                continue
            if not root.is_dir():
                continue
            try:
                relative = resolved_artifact.relative_to(root)
            except ValueError:
                continue
            candidates.append(
                f"input-{index:02d}/{relative.as_posix()}"
            )
        labels[resolved_artifact] = min(
            candidates,
            default=f"artifact/{resolved_artifact.name}",
        )
    return labels


def normalize_validator_value(
    value: object,
    artifact: pathlib.Path,
    label: str,
) -> object:
    if isinstance(value, dict):
        return {
            str(key): normalize_validator_value(
                nested, artifact, label
            )
            for key, nested in sorted(
                value.items(), key=lambda item: str(item[0])
            )
        }
    if isinstance(value, list):
        return [
            normalize_validator_value(nested, artifact, label)
            for nested in value
        ]
    if isinstance(value, str):
        normalized = value.replace(
            artifact.as_posix(), label
        )
        normalized = normalized.replace(str(artifact), label)
        return normalized.replace("\\", "/")
    return value


def accepted_custom_key_warnings(
    parsed: object,
) -> list[str] | None:
    if not isinstance(parsed, dict):
        return None
    messages = parsed.get("messages")
    if not isinstance(messages, list):
        return None
    keys: list[str] = []
    for message in messages:
        if (
            not isinstance(message, dict)
            or message.get("id") != 7010
            or message.get("type") != "error"
            or message.get("message")
            != "Custom key in Key/Value Data."
            or not isinstance(message.get("details"), str)
        ):
            return None
        match = re.fullmatch(
            r'Custom key "([^"]+)" found in Key/Value Data\.',
            message["details"],
        )
        if match is None:
            return None
        keys.append(match.group(1))
    if (
        len(keys) != len(ALLOWED_CUSTOM_KEYS)
        or len(set(keys)) != len(keys)
        or set(keys) != ALLOWED_CUSTOM_KEYS
    ):
        return None
    return sorted(keys)


def validate(
    executable: str,
    artifacts: Sequence[pathlib.Path],
    timeout_seconds: float,
    labels: Mapping[pathlib.Path, str] | None = None,
) -> tuple[list[dict[str, object]], bool]:
    records: list[dict[str, object]] = []
    passed = True
    resolved_labels = {
        artifact.resolve(): label
        for artifact, label in (labels or {}).items()
    }
    ordered_artifacts = sorted(
        (artifact.resolve() for artifact in artifacts),
        key=lambda artifact: resolved_labels.get(
            artifact, artifact.as_posix()
        ),
    )
    for artifact in ordered_artifacts:
        label = resolved_labels.get(
            artifact, f"artifact/{artifact.name}"
        )
        record: dict[str, object] = {"artifact": label}
        try:
            result = run_command(
                [
                    executable,
                    "validate",
                    "--format",
                    "json",
                    "--warnings-as-errors",
                    artifact.as_posix(),
                ],
                timeout_seconds,
            )
            record["exitCode"] = result.returncode
            try:
                parsed = json.loads(result.stdout or "{}")
                record["validator"] = normalize_validator_value(
                    parsed, artifact, label
                )
                accepted_keys = accepted_custom_key_warnings(
                    parsed
                )
                if accepted_keys is not None:
                    record["acceptedCustomMetadataWarnings"] = (
                        accepted_keys
                    )
            except json.JSONDecodeError:
                record["error"] = "malformed-json"
                passed = False
                accepted_keys = None
            if result.returncode != 0 and accepted_keys is None:
                passed = False
        except subprocess.TimeoutExpired:
            record["error"] = "timeout"
            passed = False
        records.append(record)
    return records, passed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ktx", default="ktx")
    parser.add_argument("--expected-version", default="4.4.2")
    parser.add_argument("--input", action="append", type=pathlib.Path, required=True)
    parser.add_argument("--report", type=pathlib.Path, required=True)
    parser.add_argument("--timeout-seconds", type=float, default=30.0)
    parser.add_argument("--allow-missing-tool", action="store_true")
    args = parser.parse_args()

    report: dict[str, object] = {
        "schema": "stoner.ktx2.validation.v1",
        "expectedVersion": args.expected_version,
    }
    try:
        version = tool_version(args.ktx, args.timeout_seconds)
    except FileNotFoundError:
        report["status"] = "tool-unavailable"
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return 0 if args.allow_missing_tool else 2
    except (RuntimeError, subprocess.TimeoutExpired):
        report["status"] = "tool-error"
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return 2

    report["toolVersion"] = version
    if args.expected_version not in version:
        report["status"] = "version-mismatch"
        passed = False
        records: list[dict[str, object]] = []
    else:
        artifacts = discover(args.input)
        labels = build_artifact_labels(args.input, artifacts)
        records, passed = validate(
            args.ktx, artifacts, args.timeout_seconds, labels
        )
        report["status"] = "passed" if passed and artifacts else "failed"
        report["artifactCount"] = len(artifacts)
        if not artifacts:
            passed = False
    report["artifacts"] = records
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
