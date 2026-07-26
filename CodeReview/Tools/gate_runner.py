"""Allow-listed build and test profiles for review gates."""

from __future__ import annotations

import platform
from pathlib import Path
from typing import Any

from reviewlib import ReviewError, run, utc_now


PROFILES: dict[str, list[list[str]]] = {
    "debug": [["scons", "config=debug"]],
    "release": [["scons", "config=release"]],
    "strict-debug": [["scons", "config=debug", "strict=1"]],
    "strict-release": [["scons", "config=release", "strict=1"]],
    "cli-tests": [
        [
            "python",
            "-m",
            "unittest",
            "discover",
            "-s",
            "CodeReview/Tools/tests",
            "-v",
        ]
    ],
}


def available_profiles() -> list[str]:
    return sorted([*PROFILES, "tests", "sanitizers"])


def _test_executable() -> str:
    platform_name = {
        "Darwin": "Mac",
        "Linux": "Linux",
        "Windows": "Win64",
    }.get(platform.system())
    if not platform_name:
        raise ReviewError(f"unsupported test platform: {platform.system()}")
    executable = f"Build/{platform_name}/Debug/Tests/StonerTest"
    return executable + ".exe" if platform_name == "Win64" else executable


def commands_for(profile: str) -> list[list[str]]:
    if profile == "tests":
        return [["scons", "config=debug"], [_test_executable()]]
    if profile == "sanitizers":
        if platform.system() == "Windows":
            raise ReviewError("the ASan/UBSan profile requires Clang or GCC")
        asan_options = (
            "detect_leaks=1:halt_on_error=1"
            if platform.system() == "Linux"
            else "halt_on_error=1"
        )
        return [
            [
                "scons",
                "config=debug",
                "strict=1",
                "sanitizers=address,undefined",
            ],
            [
                "env",
                f"ASAN_OPTIONS={asan_options}",
                "STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1",
                "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1",
                _test_executable(),
            ],
        ]
    if profile not in PROFILES:
        raise ReviewError(
            f"unknown gate profile {profile!r}; choose from {', '.join(available_profiles())}"
        )
    return PROFILES[profile]


def execute(repo: Path, profile: str) -> dict[str, Any]:
    commands = []
    passed = True
    for args in commands_for(profile):
        result = run(args, cwd=repo, check=False, timeout=1800)
        commands.append(
            {
                "args": args,
                "returncode": result.returncode,
                "stdout_tail": result.stdout[-4000:],
                "stderr_tail": result.stderr[-4000:],
            }
        )
        if result.returncode != 0:
            passed = False
            break
    return {
        "profile": profile,
        "passed": passed,
        "recorded_at": utc_now(),
        "commands": commands,
    }
