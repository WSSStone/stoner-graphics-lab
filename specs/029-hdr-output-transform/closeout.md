# Feature 029 — maintainer exception closeout

**Status**: Complete by explicit maintainer exception  
**Decision date**: 2026-09-06  
**Tested software**: `2ee7116ffb382c021ed575aff223c7b760a2ce7d`  
**Historical Windows SDR / HDR live software**: `1f463520006d2ade3d1b4375a51ad947dd7f1847`  
**Next phase**: Feature 030 — Renderer: Anti-Aliasing & Temporal Reconstruction

## Explicit authority and scope

The maintainer first said: “不需要，跳过。是不是可以直接收尾029？”

The agent then asked the complete scoped question:

> 你是否同意接受刚展示的两张 macOS SDR Candidate，并将此前 HDR 人眼接受结论作为本次维护者特批沿用，豁免重复展示及原定独立 attestation 要求？确认后即可如实记录例外、更新 roadmap 并完成 029 收尾。

The maintainer replied: **“agree”**.

This records an explicit human governance decision, not an inferred image
judgment or an automatically generated HDR attestation. The preceding response
also explicitly identified the Windows rerun as skipped and explained that this
closeout would not mean every original gate passed. The decision is limited to
this software revision and does not waive future Feature 030/other revisions'
fresh evidence, image-review, or HDR human-authority requirements.

## Evidence and dispositions

| Obligation | Actual result / approved disposition |
|---|---|
| T101 / T103 macOS SDR | Fresh exact-`2ee7116` physical M4 Metal Lantern/Sponza v3 Candidates; the maintainer now accepts both. Source Candidate/PNG/calibration/probe bytes stay unchanged; only the separate Accepted registry and decision record are added. |
| T102 Windows SDR | Current-`2ee7116` physical rerun explicitly waived. The accepted `1f46352` physical Windows bundles and session/adapter limitations are retained. No Windows hardware run at `2ee7116` or Windows HDR validation is claimed. |
| T104 HDR machine | Fresh exact-`2ee7116` +3 EV PQ1000/PQ2000/EDR1000/EDR2000 hidden-background native runs all passed 1,000 cycles / 20 warmups, same-frame command/readback/present and zero terminal owners. The machine request remains `ready-for-live-review`. |
| T105 HDR human | The maintainer expressly authorizes one-time use of the `1f46352` +3 EV live acceptance (“可以接受。关闭它们”). Repeat live viewing and the separate manually authored attestation are waived. No new current-SHA live observation or synthetic attestation is created. |
| T112 hosted CI | Run [34002580090](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34002580090), attempt 1, passed 14/14 at `2ee7116`, including strict builds, sanitizers and machine producer/consumer/aggregate. |
| T106 / T118 aggregate | Closed by the explicit exception, not by a successful strict same-SHA physical/human aggregate. The unchanged strict verifier still rejects missing current Windows reports and independent HDR attestations. |
| T117 documentation | Roadmap, Feature 002, AGENTS, requirements/tasks and delivered HTML record completion and the exception; Feature 030 is next. |

There are **118 closed task dispositions: 114 completed as scoped and four
exception dispositions (T102/T105/T106/T118)**. Checked exception tasks mean
resolved obligations, not tests that ran or passed. The dated raw summaries'
earlier `featureComplete=false`, pending acceptance and live-review states are
historically correct and remain immutable; this later decision does not rewrite
them.

Current and historical M4 SDR image digests and +3 EV HDR readback digests match.
That is supporting machine provenance only, not an automated visual decision,
a physical-display equivalence claim, or the authority for this exception.
The authority is the maintainer's explicit reply above.

## Bounded records

- [Machine and human evidence index](../../Validation/029/CI/README.md)
- [Closeout decision and evidence digests](../../Validation/029/CI/closeout-2ee7116-20260906.json)
- [US5 disposition, not a strict pass](../../Validation/029/CI/us5-authority.json)
- [Current M4 SDR acceptance](../../Validation/029/SDR/M4-Metal/acceptance-2ee7116-20260906.json)
- [Original Windows SDR acceptance](../../Validation/029/SDR/Windows-Vulkan/acceptance-1f46352-20260904.json)
- [Original +3 EV HDR live feedback](../../Validation/029/HDR/ReviewHistory/1f46352-ev3-20260904-01/maintainer-feedback.json)
- [Current HDR machine request](../../Validation/029/HDR/Requests/2ee7116-ev3-background-20260906-02/hdr-live-review-request.json)

No image alignment, cropping, scaling or resampling was applied. No HDR image
authority was created. Raw logs, binaries, DDC and cooked payload remain outside
checked-in evidence; new governance records are bounded JSON/Markdown only.
The CI input-registry snapshot remains unchanged; post-run SDR acceptance is a
later authority change, not a claim that the CI consumed the expanded registry.

Feature 028 v2 `sampleCount=1` / no-general-post-processing evidence remains
untouched. Feature 031 still depends only on 024/025/026/028. Feature 030 owns
pre-tonemap TAA, post-tonemap FXAA and the shared temporal framework reused by
Feature 039. No renderer, shader, verifier, schema, threshold, or CI workflow
implementation is changed by this closeout; no general waiver switch is added.
