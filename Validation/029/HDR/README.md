# Feature 029 HDR review and endurance history

These records identify tested software
`1f463520006d2ade3d1b4375a51ad947dd7f1847`. They are not evidence for a later
software revision and do not establish Feature completion.

## Live review at +3 EV

`ReviewHistory/1f46352-ev3-20260904-01/` preserves thirteen original JSON files
byte-for-byte from ignored
`Build/Validation/029/m4-formal-1f46352-20260904-01/exposure-plus3-review-01/`.
This prefix mapping also applies to the original preflight reports' artifact
paths. Those paths and report/request digests have deliberately not been
rewritten; raw commands and logs remain in Build. For a fresh-clone audit,
restore copies under that original ignored prefix before using the ordinary
report verifier. The archive itself is not a replacement formal request.

The four short native preflights completed at manual exposure +3 EV (8x before
the viewing transform), 512x512, sampleCount=1. The maintainer then replied
"可以接受。关闭它们". `maintainer-feedback.json` records that explicit conversation
feedback, scoped only to this +3 EV live PQ/EDR review. It is not a
maintainer-authored attestation and does not admit the zero-EV request or any
SDR Candidate. The top-level zero-EV request remains unchanged.

The request SHA-256 is
`1ec4de438c8fd72b1306584f7c31fcd345d35bef9ff74635f50b46274ed63e35`;
the settings SHA-256 is
`29e864221a980b16a7c5d1388ca57b099d9c0a4be7d585e91415a108ca57fedd`.
The four foreground long replays did not publish terminal probes and remain
incomplete in `live-run-summary.json`. A close request can propagate to the
generic readback-failure message, but the old logs do not prove that it was
the cause. These attempts have not been relabeled as successful.

## Background endurance rerun

`Endurance/1f46352-ev3-background-20260904-02/summary.json` indexes four fresh
native probe/report pairs: PQ1000, PQ2000, EDR1000, and EDR2000. Each completed
1,000 lifecycle cycles after 20 warmup cycles, sequentially, with the same
+3 EV configuration. All four reached frame token 2001 with command,
readback, and presentation complete, no first failure, and zero outstanding
terminal owners. Elapsed times were 151.978, 137.156, 137.643, and 141.339
seconds respectively. The four readback digests match their previously
reviewed +3 EV preflight outputs; this is numerical linkage, not a new visual
decision or a measurement of panel luminance.

LaunchServices `open -g -j` kept the applications hidden in the background.
They were not minimized: minimized/zero-drawable state pauses presentation
and cannot demonstrate an active presentation endurance run. All applications
exited after their bounded run. No display settings were changed, and no HDR
screenshots, PNGs, videos, or per-frame dumps were created. The endurance bundle
is nine JSON files totaling 18,019 bytes. Its command/log references identify
ignored raw diagnostics, not checked-in artifacts. Its prior-feedback reference
has an identical archived copy in ReviewHistory above.

Per-process capability digests are preserved independently; they include the
native-window SurfaceId and current display headroom. Equality across separate
windows is not required. PQ keeps BGR10A2Unorm, ITU-R 2100 PQ, EDR opt-in,
Core Animation color management, and EDRMetadata=nil. Extended-linear EDR also
keeps EDRMetadata=nil. Neither path enables CAEDRMetadata system tone mapping.

T105 still requires the separate maintainer-authored immutable attestation.
Background machine success cannot author, infer, or replace that decision.
