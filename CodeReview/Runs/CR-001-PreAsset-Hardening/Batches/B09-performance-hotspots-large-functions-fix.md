# B09-S11 Fix: Performance Hotspots And Large Functions

## Scope

Follow-up to B09-S10. The accepted finding was:

- `CR001-B09-F005`: Native deferred validation is concentrated in oversized execution functions

## Fix Decision

`CR001-B09-F005` was deferred, not refactored in this step.

Reason: the issue is S3 maintainability debt with existing native validation coverage. Splitting `FVulkanNativeOffscreenSession::Execute()` would require a high-touch refactor across resource setup, descriptor/pipeline setup, command recording, readback mapping/decoding, probe oracle construction, and report finalization. That exceeds the CR policy for low-risk S3 fixes inside this batch.

Deferred target: post-CR native validation refactor before or alongside Feature 020/021 asset validation work.

## CR Tool Fix

While deferring the finding, `crctl` exposed an internal protocol mismatch: the CR charter allows low-risk S3 fixes and explicit deferral of the rest, but the finding transition table allowed `Deferred` only from `Triaged`, not from an already accepted S3 finding.

Changed:

- `CodeReview/Tools/crctl.py`: allow `Accepted -> Deferred` in `FINDING_TRANSITIONS`.
- `CodeReview/Tools/tests/test_crctl.py`: add coverage for accepted S3 debt moving to `Deferred`.

## Verification

- `python -m unittest CodeReview.Tools.tests.test_crctl`: passed, 6 tests.
- `conda run -n stoner-cr python -m unittest discover -s CodeReview/Tools/tests`: passed, 21 tests.
- `python CodeReview/Tools/crctl.py lint --id CR-001`: passed before deferral.
- `python CodeReview/Tools/crctl.py --id CR-001 finding defer CR001-B09-F005 ...`: passed after the transition fix.

## Result

`CR001-B09-F005` is Deferred with an explicit target and reason. No engine source, build script, or runtime test behavior changed in this step.
