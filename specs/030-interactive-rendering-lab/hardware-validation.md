# Feature 030 current hardware validation

This is the execution handoff, not a hardware pass. Intel x86_64 macOS is explicitly skipped by the maintainer's 2026-09-08 decision. The 029 exceptions do not apply.

## Freeze and staging

Use the exact commit in `Validation/030/CI/software-freeze.json`, build strict Release from that checkout, and keep source and policy unchanged until every run ends. If unrelated untracked source tools make the embedded Demo revision end in `+working-tree`, use a clean checkout/worktree at that commit; never edit report revision bytes. The runner checks the checkout before and after execution and requires the executable to report the same revision.

Use `Config/Validation/InteractiveLab/Cook/Lantern.json` and `Sponza.json`: pass each `sourceRoots` entry as `--source-root` and each `roots` entry as `--root` to `StonerAssetCooker cook`, with `--target-profile`, `--output`, `--ddc`, and `--workers 2`. Use `Production/Windows-Vulkan.json` on Windows or `Production/Mac-Metal-Arm64.json` on M4. Read the exact generation from the publication's `Current.json`. Run `StonerAssetCooker validate --output PUBLICATION --strict-files` before launching. Stage the hash-pinned Sponza package with the existing `acquire_production_corpus.py --package khronos-sponza-gltf --content-root Content/ProductionAcceptance`; asset license decisions remain the maintainer's responsibility.

Windows follows [029 Windows handoff](../029-hdr-output-transform/windows-handoff.md): physical discrete Vulkan hardware, active Console or RDP session, local NTFS cooked/lease/validation paths. Preserve `SESSIONNAME` in the report. RDP proves application GPU/window output only, not physical scanout or Console equivalence. M4 uses the actual Metal adapter. Hosted runners and software Vulkan do not substitute for these lanes.

## One command file per fixed case

Select a case ID from `Config/Validation/InteractiveLab/Coverage-v1.json`. There are four endurance, eight additional smoke and seven lifecycle cases; do not multiply the profile/endurance matrix. The command JSON has `caseId`, `gateKind`, `timeoutSeconds` (10–600), and a `nativeCommand` array. Example shape for `macos-metal-lantern-sdr-smoke`:

```json
{"caseId":"macos-metal-lantern-sdr-smoke","gateKind":"smoke","timeoutSeconds":600,"nativeCommand":["Build/Mac/Release/Demo/StonerDemo/StonerDemo","--interactive-lab","--lab-ui","on","--mode","validate","--frames","120","--width","320","--height","180","--backend","metal","--workload","production-content","--render-path","deferred-full","--cooked-root","Build/lab-publication","--strict-generation","REPLACE_WITH_CURRENT_GENERATION","--production-root","StaticModel:Lantern.glb#idx.scene.0","--workload-revision","production-content-lantern-v3","--lease-root","Build/lab-leases","--target-profile","Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json","--output-device-profile","Sdr.sRGB.v1","--output-transform-version","Sdr.KhronosPbrNeutral.v1","--lab-report","Build/Validation/030/native-new.json"]}
```

Create every output parent and lease-coordination directory first. Use absolute cooked-root, lease-root and target-profile paths. Every native/wrapper/request filename must be fresh. For Windows replace executable/platform/backend/profile and use the case's exact Sponza or Lantern root/workload. Set `--frames 1120` for each endurance case (120 warmup + 1000 measured), or 120 for smoke. For HDR use the case's exact PQ/Linear profile and `Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1` transform. Leave captures disabled. Windows Lantern smoke adds `--lab-vulkan-retirement acquire-history`; missing optional maintenance1 never blocks otherwise supported execution.

```sh
python .github/scripts/interactive_lab_validation.py run --command-file Build/Validation/030/command.json --git-revision EXACT_40_HEX_COMMIT --output Build/Validation/030/report-new.json
python .github/scripts/interactive_lab_validation.py verify --report Build/Validation/030/report-new.json
```

The report separates actual target-profile present-queued frames from submissions and warmup; it claims no scanout. AcquireHistory records `IdleAssumed` and the explicit terminal-idle compatibility limitation. Forced exit, timeout, nonzero quiescent owners or readbacks fail.

## Lifecycle recipes

For lifecycle command files add `--lab-input-script` and use a sufficient bounded frame budget (1120 for stress; 120 for one-cycle integration). Generate an ordered script with `schemaVersion: 1` and at most 256 steps; use `afterPresented: 0` for each step to run sequentially with the built-in settle guard. Settings must become effective, and each non-minimize/close action allows three subsequent present-queued frames before another step runs.

For each Sponza stress cycle append: resize to alternating 320x320 / 640x360, ui 0 then 1, focus 0 then 1, minimize 0 then restore with the selected width/height, scale 2 then 1. Repeat 20 times (180 steps). `resize`/`restore` use `value` width and `value2` height. Scale uses the validation-only persistent content-scale override; focus/minimize/restore are typed injections, not physical monitor/window actions. Human review separately exercises actual focus/minimize/input.

For mode stress append each `profileSequence` after its initial entry, repeat 20 times. Windows then appends `rejectProfile` for its declared unavailable HDR profile. Remaining integration cases append only their `requiredAdditionalCycles`, with Windows Lantern using the SDR sequence sRGB → BT709 → ExplicitGamma22 → sRGB once. The verifier checks the script digest, completion and required cycles. A five-second no-progress deadline is a failure, not coverage.

## Formal and current human gates

Use [029 formal commands](../029-hdr-output-transform/quickstart.md) for exact-dimension, frozen-camera UI-disabled Lantern and Sponza captures on both physical lanes. Do not pass interactive, preset or UI-smoke overrides. Run inherited Candidate/calibration/probe validation against the applicable Accepted reference. Any difference requires a workload revision and explicit acceptance; no alignment, cropping, scaling or tolerance changes.

A successful physical command may additionally name `humanGateId` and `humanRequest` to emit a bounded request linked to its wrapper digest, actual adapter, output profile, frame/settings, white and exposure. The runner never writes a decision. The maintainer reviews navigation/control isolation, cursor release, readability, preset restore at a changed aspect, output/exposure controls and actual focus/minimize/exit. On M4 inspect all four HDR profiles for both workloads, with scene appearance and UI brightness/readability observations. A current human decision contains schema `stoner.interactive-lab-human-decision`, version 1, exact `gitRevision`, matching `gateId`, request SHA-256, `decision` accepted/rejected, `reviewer`, `observedAt`, and `observations`. Preserve the maintainer's words; do not infer approval from machine checks or old feedback.

Assemble a `stoner.interactive-lab-bundle` v1 manifest with `gitRevision`, digest-linked `artifacts`, 19 machine `reports`, four `formalSdr` entries (`gateId`, `report`, `baseline`), twelve `humanDecisions` pairs (`request`, `decision`), and thirteen `hosted` job records. Referenced reports retain their own artifact links. Each JSON is at most 1 MiB; retain only bounded PNG/JSON evidence. Run:

```sh
python .github/scripts/interactive_lab_validation.py closeout --manifest Build/Validation/030/bundle.json --git-revision EXACT_40_HEX_COMMIT --output Build/Validation/030/aggregate-new.json
```

Missing or rejected gates remain incomplete. The implementation owner reruns failed machine cases at the frozen commit; the maintainer owns Windows machine access and current hands-on decisions. If a required native environment is unavailable, retain the failed/unavailable report and rerun the exact command above on the required machine. Never substitute hosted, historical or Intel-skipped evidence. T114–T123 track these follow-ups; no completion is claimed by this guide.
