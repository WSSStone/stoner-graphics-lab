# CR-001 Baseline Summary

## Repository

- Frozen baseline: `9092a97593fb29cffbffdbe534e3dda143f463a5`
- Review bootstrap HEAD: `e5bd56c9d83f09944fd69561bc97c480b69cd152`
- Production source lines: 24,078
- Test source lines: 7,445
- Features 003-019 requirements and success criteria: 463

## CodeGraph

- CodeGraph: 1.5.0
- Indexed files: 338
- Nodes: 4,440
- Edges: 12,305
- Project C/C++ inventory (`Source`, `Tests`, `Demo`): 308
- Indexed project C/C++ files: 308
- Coverage: 100%
- Missing/unexpected project C/C++ files: 0/0

The previous main-worktree index covered only 56 files. CR-001 initialized a
fresh worktree-local index and verified coverage by comparing explicit file
paths, not by trusting the aggregate CodeGraph file count.

## Local macOS Gates

- Debug build: pass, with 33 compiler warnings.
- Release build: pass, with the same 33 compiler warnings.
- First Debug test run: fail in three Feature 019 native-readback checks.
- Two immediate controlled repeats: pass with all probes inside tolerance and
  `final_live_objects=0`.
- CR CLI tests: 10/10 pass under Python 3.12.13 in `stoner-cr`.
- Architecture include scan: no violations found. This is a conservative
  include-direction scan, not proof that all runtime ownership boundaries hold.

The native failure is therefore intermittent and remains an accepted B08
finding. The warning baseline remains an accepted B01 finding; a later clean
rerun does not erase either observation.

## Remote Gates

Draft PR #4 at `e5bd56c9d83f09944fd69561bc97c480b69cd152`
passed Windows, macOS, and Linux headless CI. Linux included Lavapipe native
Feature 018/019 validation. The initial duplicate check records came from the
PR-creation and follow-up state pushes; the CR-tool workflow is now limited to
changes under `CodeReview/Tools/**` and no longer runs for progress-only edits.
