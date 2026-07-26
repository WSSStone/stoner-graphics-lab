# CR Tools

The review CLI is intentionally standard-library Python. Runtime build tooling
is pinned in `requirements.txt`; optional analyzer adapters remain advisory
until a review decision promotes a tested version to a required gate.

## Environment

```sh
conda env create -f CodeReview/Tools/environment.yml
conda run -n stoner-cr python CodeReview/Tools/crctl.py doctor
conda env update -n stoner-cr -f CodeReview/Tools/environment.yml --prune
```

CI must create a clean Python environment from `requirements.txt`; it must not
assume Conda is present.

## Commands

- `init`: create a run from reusable templates.
- `doctor`: verify Python, environment, requirements, Git, and CodeGraph.
- `baseline`: record repository and specification inventory.
- `status`, `next`: report state and issue one bounded step.
- `start`, `complete`, `fail`, `recover`: manage durable execution.
- `trace`: seed FR/SC traceability.
- `finding`: maintain finding lifecycle.
- `render`: regenerate Markdown views from JSON.
- `gate`: run a named, predefined build/test profile.
- `lint`: validate state, IDs, transitions, evidence, and traceability.
- `close`: enforce all completion conditions.

No command accepts an arbitrary shell string. Child processes use argument
arrays with `shell=False`.

Project build gates include `strict-debug`, `strict-release`, and `sanitizers`.
The sanitizer profile combines ASan and UBSan on Clang/GCC and intentionally
rejects MSVC, where the same combined profile is unavailable:

```sh
python CodeReview/Tools/crctl.py gate --id CR-001 strict-debug
python CodeReview/Tools/crctl.py gate --id CR-001 strict-release
python CodeReview/Tools/crctl.py gate --id CR-001 sanitizers
```

The sanitizer test run explicitly skips optional deferred driver execution.
The regular Linux Lavapipe native gate remains mandatory and cannot be skipped
when `STONER_REQUIRE_DEFERRED_NATIVE=1`; this keeps driver timing outside the
instrumented deterministic gate without weakening native validation.

## Tests

```sh
python -m unittest discover -s CodeReview/Tools/tests -v
```

Reusable scripts used by two or more batches belong here with documentation and
tests. One-off probes belong in a run's `Evidence/output/` directory and must be
removed or justified during closeout.
