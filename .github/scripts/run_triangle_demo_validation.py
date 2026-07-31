#!/usr/bin/env python3
"""Run Feature 018 validation profiles with stable watchdog and report checks."""

import argparse
import os
from pathlib import Path
import subprocess
import sys


def run(command, timeout_seconds, env=None):
    print('+', ' '.join(str(item) for item in command), flush=True)
    try:
        return subprocess.run(command, env=env, timeout=timeout_seconds, check=False).returncode
    except subprocess.TimeoutExpired:
        print(f'ERROR: command exceeded configurable watchdog ({timeout_seconds}s)', file=sys.stderr)
        return 124


def validate_report(path, native_required):
    if not path.is_file():
        print(f'ERROR: validation report missing: {path}', file=sys.stderr)
        return False
    text = path.read_text(encoding='utf-8')
    required = (
        'feature=018-triangle-demo-integration',
        'run-id=',
        'requested-frames=4096',
        'completed-frames=4096',
        'memory-samples=',
        'final-live-objects=0',
        'validation-result=pass',
    )
    if any(field not in text for field in required):
        print('ERROR: validation report is missing required fields', file=sys.stderr)
        return False
    if '0x' in text:
        print('ERROR: validation report contains a forbidden native address', file=sys.stderr)
        return False
    if native_required and ('runtime-object-mode=native' not in text or 'software-device=true' not in text):
        print('ERROR: native-headless report lacks software Vulkan proof', file=sys.stderr)
        return False
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--profile', choices=('deterministic', 'native-headless'), required=True)
    parser.add_argument('--tests', type=Path, required=True)
    parser.add_argument('--demo', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--timeout-seconds', type=int, default=1200)
    args = parser.parse_args()
    if args.timeout_seconds <= 0:
        parser.error('--timeout-seconds must be positive')

    env = os.environ.copy()
    env.setdefault('STONER_DEMO_RUN_ID', env.get('GITHUB_RUN_ID', 'local-ci') + '-' + env.get('GITHUB_SHA', 'working-tree')[:12])
    if run([str(args.tests), '--suite', 'coordinate-convention'], args.timeout_seconds, env) != 0:
        return 1
    if run([str(args.tests)], args.timeout_seconds, env) != 0:
        return 1
    mode = 'headless' if args.profile == 'deterministic' else 'headless-vulkan'
    growth = '16' if args.profile == 'deterministic' else '64'
    command = [
        str(args.demo), '--mode', mode, '--frames', '4096', '--warmup-frames', '512',
        '--memory-sample-interval', '128', '--max-memory-growth-mib', growth,
        '--validation-output', str(args.report),
    ]
    result = run(command, args.timeout_seconds, env)
    if result != 0:
        return result
    return 0 if validate_report(args.report, args.profile == 'native-headless') else 1


if __name__ == '__main__':
    raise SystemExit(main())
