# SDR reference provenance correction

The maintainer approved the correction and a new freeze on 2026-09-17. The source branch is `codex/030-sdr-reference-provenance`, based on evidence commit `95c13a2430dd2516ac9d288dd1dd34bc0d11c1a6`. The previous software checkout at `74e1c435ecd79056ef3d72c563ab4cd865a6ca19` is unchanged. Its original failure is retained in `Validation/030/SDR/Windows-Vulkan/74e1c435-formal-20260916/closeout-binding-diagnostic.json`.

The fix replaces path equality with independent bounded verification of the unchanged Accepted PNG. Fresh Candidate metadata, PNG digests, calibration revision and same-frame probe linkage remain mandatory. Neither Accepted records nor image thresholds change. Regression tests cover different paths with identical content, missing/corrupt/changed references, an escaping reference path, stale calibration and altered native readback.

A new freeze will be recorded in a separate evidence commit after implementation tests. All thirteen hosted, five Windows and fourteen M4 cases require the new exact SHA, followed by formal SDR and current human review. Do not use this branch tip without checking the new freeze manifest. Previous human replies apply only to 74e1c435. M4 execution requires the maintainer; preserve the previous first mode-transition failure and its passing rerun as history.
