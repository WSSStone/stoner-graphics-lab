# Windows formal SDR and current human review

Tested software: `74e1c435ecd79056ef3d72c563ab4cd865a6ca19`, retained unchanged. The maintainer authorized this run after reporting the same-SHA M4 matrix complete; its new originals were not yet available on the remote evidence branch (last observed `f455d1a`). Do not replace or discard the reported first M4 mode failure or its same-condition passing rerun when those originals arrive.

The physical RTX 3080 ran in unlocked Console session 1 on local NTFS. Both Lantern and Sponza v3 completed fresh three-process calibration (20 captures per process, one stable mode, existing eight mutations rejected), guarded visible native capture, exact 512x512 sampleCount=1 SDR Candidate generation, independent evidence verification, and comparison to the unchanged existing Accepted PNG. Both native probes link readback and presentation frame 41 and zero terminal owners. Formal arguments contain no interactive/UI/preset overrides; sRGB, KhronosPbrNeutral, zero exposure and frozen camera inputs are preserved. No scanout authority is asserted.

Both comparisons are pixel-exact with zero differing channels. Candidate PNG file SHA-256:

- Lantern: `f67c022b9aa01cd3986610a235efe62d04699f595d24ec53a64d0e06d2767ac5`
- Sponza: `6d93309e0bd939a85e6c8bcf6d4251de6e1e6b54b0ce25699f1867648a982950`

The new PNG bytes equal the existing Accepted PNG bytes. Candidate state remains `candidate` with `acceptance=null`; registry SHA-256 stays `9e13dfc7d51658a8940f80a9109e27981d739fe6d5b378125b7fc03ac605d7ab`. No tolerance, spatial operation, Accepted field, source or policy was changed. Vector, output architecture, roadmap and diff checks pass. Original bounded files and SHA-256 are indexed in `inventory.json`; raw absolute argv, logs and PPM remain under ignored `Build/Validation/030/formal-v2-20260916`.

## Independent closeout limitation

The existing frozen closeout consumer rejects both new report bundles with `Accepted referencePath does not identify the verified PNG`. It requires the new PNG's filesystem path to equal the original Accepted path, despite identical PNG bytes and matching Candidate record fields other than state/acceptance/referencePath. `closeout-binding-diagnostic.json` retains the actual exit and both failures. Original Accepted records are copied without modification solely for independent consumption. No path relocation, copied acceptance, promotion or consumer-policy edit was used to bypass this result. T119/T120 overall and full closeout remain open; resolving consumer semantics needs a separate explicit decision, and any tested policy/code change requires a new freeze and coordinated reruns.

## Current hands-on review

After formal captures finished, the maintainer operated separate Lantern and Sponza interactive windows. Both Lantern and Sponza exited with code 0, `Proven` shutdown and zero final native/presentation owners. Their presentation counts were 9,684 and 62,196 respectively. The maintainer also confirmed Sponza closed normally: “正常关闭。继续下一步”. The replies are preserved verbatim in the human evidence directory:

- Lantern: “通过”
- Sponza: “sponza没问题”

These are maintainer acceptance decisions, not automation-authored observations. No per-item narrative or personal signature was supplied; reviewer attribution is the conversation maintainer role. Current interactive native reports and before/after sessions accompany the original machine-linked requests. M4 human decisions are not inferred. Windows current human review and exit evidence are complete; T121 retains other-platform review; full feature completion is not claimed.

The complete transfer snapshot is indexed by `transfer-manifest-complete.json` and `transfer-package-complete.json`, including both interactive exits. The earlier transfer manifest/package describe only the preserved earlier ZIP snapshot, not the current working-tree files.
