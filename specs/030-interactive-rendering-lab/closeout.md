# Feature 030 — maintainer acceptance closeout

Status: **Complete by explicit maintainer exception**, 2026-09-17. Tested software: `6a9e5df4be780b31a8e3a9ec3ee18b35f0512838`. Evidence commit: `e7a6233ee2550629e3f0cf033c96ce2c4b1eee26`.

## Maintainer authority

> 不要严格closeout；通过；windows正式SDR我都给通过了。
> 进入下一个阶段

The maintainer accepts Feature 030, confirms Windows formal SDR acceptance and explicitly removes strict closeout as a prerequisite for advancing. This decision applies only to this feature. It does not relax future feature acceptance rules.

## Evidence and disposition

- Hosted 13/13, Windows physical machine 5/5 and M4 fixed machine 14/14 passed at the frozen revision.
- M4 Lantern/Sponza calibration, probes and exact 512×512 Candidates completed; both match existing Accepted pixels with zero differing channels. The retained Sponza first failure and unchanged retry remain original evidence.
- Ten current M4 human decisions preserve actual maintainer words and request digests, covering both navigation/input/preset/lifecycle reviews and all eight scene/HDR-profile reviews.
- Windows formal SDR is accepted by the maintainer's present statement. Archived formal captures at `74e1c435` retain their historical revision; this decision does not relabel them as `6a9e5df4` captures. No new Windows capture or per-request human observation is invented. Remaining separate Windows decision collection is resolved by the overall approval to advance.
- Strict closeout still rejects historical Accepted/current Candidate capability identity and the archived partial aggregate remains incomplete. Those records are retained unchanged. Overall completion is the separate maintainer decision, not an automated all-gates success.
- Intel macOS stays skipped. Windows RDP and AcquireHistory/IdleAssumed limitations remain recorded.

No source, validator, tolerances or Accepted baselines change. T119/T120/T121/T123 are resolved by the scoped maintainer exception; T122 has actual current human evidence. Documentation tasks remain separately tracked and are not represented as completed testing.

## Next phase

Feature 031 — Anti-Aliasing & Temporal Reconstruction is now the active next phase. See [entry handoff](../031-anti-aliasing-temporal-reconstruction/entry-handoff.md). Start with motion-vector conventions, previous-state lifecycle and deterministic FXAA; TAA precedes tone mapping, FXAA follows it. Reuse Feature 030 controls and Feature 029 output ownership.

Decision: `Validation/030/CI/maintainer-closeout.json`.
