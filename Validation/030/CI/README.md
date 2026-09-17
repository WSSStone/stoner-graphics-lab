# Current reference-provenance freeze

Current software: `6a9e5df4be780b31a8e3a9ec3ee18b35f0512838`; hosted run [35193398574](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/35193398574) passed 13/13 and all five physical Windows RDP cases passed independent consumption. T114/T115 are complete. M4, formal SDR and current human evidence remain pending. Previous freeze summaries are retained under `History/74e1c435-before-reference-fix`; earlier sections below describe historical results.

# Latest Windows formal / human follow-up

Both Windows v3 formal captures and comparisons passed with identical existing Accepted pixels; both current Windows hands-on decisions were supplied by the maintainer. [Original evidence and limitations](../SDR/Windows-Vulkan/74e1c435-formal-20260916/README.md) preserve SHA-256, Console/RTX 3080, calibration/probe, and exact replies. Full closeout remains blocked by the frozen consumer's Accepted-path binding and retrieval of the newly reported M4 originals. No Accepted or frozen software change was made.

---

# Current capability-v2 freeze — 2026-09-16

Current tested software: `74e1c435ecd79056ef3d72c563ab4cd865a6ca19` on `codex/030-sdr-capability-validation` (pushed to origin with maintainer approval).

The Vulkan mapping now preserves sRGB/BT709/PassThrough identities. Windows lifecycle requests use actual native capabilities and prove successful presentation or unsupported rejection without settings/native-output mutation. Coverage-v2/native Report-v2 distinguish actual switches, retained-current requests and expected rejections; the fixed matrix and budgets remain unchanged.

Debug and strict Release implementation checks each passed 619 assertions; four Python suites passed 44 tests and both architecture checks passed. The clean frozen SHA was rebuilt in Release and passed the 619-assertion regression. Fresh Lantern and Sponza packages passed strict-files validation. Five Windows command files are prepared, including unchanged 180/61/12-step scripts.

**Acceptance remains incomplete (115/127 tasks).** All five Windows cases passed run and independent verify on the physical RTX 3080 in unlocked Console session 1. Endurance and both Sponza lifecycle cases each presented 1120 frames; Lantern smoke and integration each presented 120. All cases recorded zero captures/readbacks/live idles/final native or presentation owners. Forced AcquireHistory smoke remains **IdleAssumed**, with 117 proven releases and three pre-cleanup owners retired through compatibility terminal idle; no scanout proof is claimed.

Sponza mode stress completed all 61 requests: 41 unsupported rejections, 20 retained-current requests, zero actual switches. Lantern completed all 12 integration steps, including two unsupported rejections and one retained-current request. The real surface exposes sRGB color space only; capability-constrained request checks pass without claiming cross-mode switching. All three Windows lifecycle cases passed; T118 remains partial pending four new-SHA M4 lifecycle cases. T114 and T115 are complete after successful hosted independent verification.

The source branch was pushed after explicit approval. Hosted run [35078558137](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/35078558137) passed all **13/13** jobs at the exact frozen SHA, including independent shader consumer. All original hosted/native/script JSON bytes are archived under `35078558137/`; [inventory](35078558137/inventory.json) records their SHA-256 and restoration paths. Independent closeout verified every required hosted check and native report. The maintainer will execute fourteen M4 machine cases and return original evidence. Formal SDR awaits complete same-SHA lifecycle evidence. Two current Windows human requests are archived with no decisions.

See [checkpoint](Windows-Vulkan/74e1c435-20260916/checkpoint.json), [local verification](Windows-Vulkan/74e1c435-20260916/frozen-local-validation.json), [Windows SDR](../SDR/windows-vulkan.json), [lifecycle](../UI/physical-lifecycle.json), [aggregate](aggregate.json), and [M4/acceptance handoff](../../../specs/030-interactive-rendering-lab/capability-v2-handoff.md). Independent closeout consumes all five successful Windows reports and all thirteen hosted records, returning incomplete with 30 missing gates (14 M4 machine, 4 formal SDR, 12 human). Raw commands/logs remain in ignored `Build/Validation/030/capability-v2-20260916` in the clean frozen checkout.


Old summary bytes are preserved in [the 9154e421 history index](History/9154e421/index.json). Original hosted/M4/Windows reports, including both failed Windows cases, remain in their original artifact directories. They do not satisfy this freeze.

---

# Historical checkpoints below

# Feature 030 closeout evidence

T105–T112 implementation/preflight is complete; hosted, frozen physical and current human acceptance remain pending. `preflight.json` records working-tree checks, not final authority. Raw logs and preliminary native reports remain under ignored `Build/Validation/030`.

The thin run/verify/closeout tool reuses inherited 029 provenance and image checks. The fixed hosted matrix has thirteen required job records; it does not satisfy the nineteen physical machine cases, four formal SDR gates or twelve human gates. Intel macOS is explicitly skipped, never passed.

Follow `specs/030-interactive-rendering-lab/hardware-validation.md` for exact command files, lifecycle recipes, strict cooking, session provenance and human linkage. A missing or failed gate leaves closeout open. Final software freeze and hosted run identities will be recorded after source commit.

## Current frozen software

Software `9154e421eaaf5a01755d80736af322a1caab9215` is frozen in the clean checkout `Build/Worktrees/030-frozen-97c09338`. Run [35051927132](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/35051927132) passed all 13 required jobs, including the independent consumer. Earlier attempts retain their actual outcomes below; physical/human authority remains pending. Push and rerun authorization is already granted.

## Hosted attempt 1

The maintainer authorized execution. Commit `31519b15` was pushed and run [34963601214](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34963601214) started at that exact SHA. Linux failed because GLFW 3.3 has no `GLFW_SCALE_FRAMEBUFFER`; Windows Debug failed on concurrent compiler writes to `vc140.pdb`. The GLFW hint is now version-guarded; Windows restores the established serial build command. The original freeze is invalidated and affected validation will run again.

## Hosted attempt 2

Run [34963871700](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34963871700) at `97c09338` passed the first compilation blockers, then found GCC temporary-validator span warnings and an obsolete Asset boundary rule requiring direct yyjson compilation. Named validator lifetimes, shared-private-library verification, six boundary tests and an always-run boundary stamp address these findings. No prior result is promoted to final acceptance.

## Hosted attempt 3

Run [34964673886](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34964673886) at `c3e5742b` passed shader production, macOS focused suites and native UI drawing. Remaining failures were GNU ld GLFW ordering, a Windows local-name shadow warning, misplaced independent verification in strict jobs and an unprepared relative lease root in native commands. Corrected absolute paths plus an existing lease directory passed the same frozen Metal 120-frame command and a fresh verifier locally. The workflow test now executes all three strict-platform control flows with mocked commands and checks the actual check list. This attempt remains incomplete; no hosted aggregate or physical authority is claimed.

## Hosted attempt 4

Run [34966098179](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34966098179) at `783d135b` passed macOS Debug, Release, native and shader-producer jobs. Linux reached test compilation and rejected two Metal-only helpers incorrectly enabled whenever GLFW existed; their guard now also requires macOS. Windows remained in progress when the next corrected run superseded it, so no Windows success/failure is inferred. A local non-UTF-8 stdout fixture independently reproduced a log-reader UnicodeEncodeError; raw-byte forwarding fixes this risk. Thirteen lab-tool tests pass, including ordinary-failure versus forced-exit classification and strict schema-version types.

## Hosted attempt 5

Run [34968637036](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34968637036) at `5675abd2` passed all three macOS jobs, shader producer, medium integration and Linux TSan. Linux focused tests crashed; ASan located the empty snapshot access at `ApplicationUIDrawTests.cpp:69`. The fixture used constructor-free `ImVector::resize` for draw commands, leaving callback fields uninitialized. Explicit element initialization and guarded snapshot reads correct this undefined fixture state. Windows focused tests timed out after the same extraction assertion failed; its resolution still requires rerun. Linux native separately failed Forward terminal submission; detailed recording/end/submit/wait diagnostics are being added without relaxing its gate.

## Hosted attempt 6

Run [34979771796](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34979771796) at `d6e4244f` passed Linux Debug/Release, ASan/UBSan and TSan, all three macOS jobs, medium integration and shader producer. The ImGui fixture correction removes the Linux crash. Linux native now reports Forward terminal `end=Success`, `submit=Success`, `wait=Timeout`: the five-second offscreen fence budget expires on software Vulkan. The fixture gives observed software devices a finite thirty-second wait; hardware keeps five seconds and application lifecycle deadlines are unchanged. Both Windows jobs completed without timeout but failed the same twelve preset/artifact export assertions. Windows canonical containment required an existing candidate, so a new export destination failed before publication. The Core candidate resolver now follows the nearest existing ancestor through a native handle before appending missing components; new descendant/sibling/link checks join the hosted focused suite. The next run must verify these corrections.

## Hosted attempt 7

Run [34983045938](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34983045938) at `9357028a` passed Linux Debug/Release and both sanitizers, all macOS jobs, medium integration and shader producer. Linux Forward submission still timed out with the software wait adjustment; this is not resolved. The next bounded diagnostic records actual adapter/software classification and elapsed fence time, allowing up to 120 seconds only for software devices while keeping five seconds on hardware. Local M4 Vulkan retains 229 passing assertions and observed 0–4 ms waits. Windows Release passed all preset/artifact exports and the new missing-path/link containment cases. Two newly included Core cleanup assertions failed because a case-normalized root was compared to a case-preserving native file path; the first wrongly removed the root and caused the second failure. Both operands now use the public canonical representation. Windows Debug completed with the same two cleanup failures and no remaining export failures. Strict local Debug/Release Core, publication, preset and lifecycle regressions and architecture checks pass after canonicalizing both comparison operands.

## Hosted attempt 8

Run [34986015641](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34986015641) at `19ac6a5c` passed all C++ focused and architecture checks on Windows; only a Python fixture's slash-sensitive diagnostic comparison failed. The fixture now normalizes Windows separators. Linux native UI/Forward/Deferred pixel checks passed: llvmpipe reported approximately 98 seconds for each first output submission, then millisecond completion. The finite 120-second software-only fixture budget is retained; hardware stays at five seconds. Linux automatic presentation smoke passed with independent verification. Forced-fallback lifecycle then failed after resize because the bridge refused logical cancellation after an unsuccessful present attempt. The runtime already retains such native operations until generation retirement; cancellation now rejects published presentation leases, while allowing completed unpresented frames to release logical ownership. The fixed lifecycle remains a required next-run regression. No hosted aggregate or physical/human authority is claimed.

Local correction checks: strict Debug/Release builds, 13 Python validator tests, architecture checks and the opted-in Debug/Release Vulkan borrowed-target suites (49 assertions each) pass. A preliminary nine-step forced-fallback lifecycle completes 120 presentations with zero live queue/device idles and zero final native owners; its terminal assurance remains explicitly `IdleAssumed`. Linux resize/present cancellation remains subject to the next hosted run.

## Hosted attempt 9

Run [35049316427](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/35049316427) tests `8eb5c5453bed20753f0e88726985a0cf6c62da3d`. Linux/macOS Debug/Release, ASan/UBSan, TSan, both native jobs, medium integration and shader producer have passed; Both Windows configurations subsequently passed; all twelve initial jobs succeeded. The dependent shader consumer failed strict compilation because a GLFW-only Metal test variable was declared outside its feature guard. The declaration now belongs to the guarded branch; all 168 test translation units pass strict no-GLFW/no-Vulkan syntax compilation using their SCons-defined private include paths and macros, and the full local Release build passes. A new freeze and hosted run are required. Downloaded Linux automatic/fallback, macOS Lantern and Linux Sponza wrappers pass the independent local consumer with original artifact bytes. Linux automatic and fallback each present 120 frames with zero live queue/device idles and zero final native owners; fallback completes all nine lifecycle steps and retains the explicit `IdleAssumed` terminal limitation. The frozen clean Release build and all 82 policy identities are verified. T114 remains open until every required job passes.

## Hosted attempt 10 — passed

Run [35051927132](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/35051927132) at `9154e421eaaf5a01755d80736af322a1caab9215` passed 13/13 jobs. All downloaded hosted records pass the independent closeout consumer with no missing hosted gate; physical/formal/human gates remain open. [hosted-matrix.json](hosted-matrix.json) records actual job URLs and SHA-256 digests of original bounded JSON artifacts under `35051927132/`. To reverify, reconstruct each artifact at its `originalPath` under a verification root containing the frozen Coverage/Limits files, then run the existing verify/closeout consumer. No report bytes were edited. T114 is complete (114/127 reviewed).

## Current physical checkpoint and remaining gates

At frozen `9154e421`, the guarded producer and independent consumer passed all 14 fixed M4 Metal machine cases: [SDR](../SDR/macos-metal.json), [HDR](../HDR/macos-metal-matrix.json) and the M4 portion of [lifecycle](../UI/physical-lifecycle.json). All report zero image readback copies/maps/waits, zero live queue/device idles and zero final native owners, with `Proven` terminal assurance. T116/T117 are complete; T118 remains partial for Windows. Intel macOS remains skipped.

The existing closeout consumer ran against [partial-bundle.json](partial-bundle.json) and produced the deliberately incomplete [aggregate.json](aggregate.json): five Windows machine cases, four formal SDR gates and twelve current human decisions remain missing. Windows hardware access has been requested; no substitute hardware or historical authority is used. Formal SDR follows completion of the full lifecycle gate. Ten M4 [human requests](../UI/human-requests.json) are prepared, with no decisions authored. Follow the [hardware handoff](../../../specs/030-interactive-rendering-lab/hardware-validation.md) after Windows access is available.

All tracked machine evidence retains original producer bytes. Reconstruct their `originalPath` values from the artifact inventories in the hosted/SDR/HDR/lifecycle/request indexes under a verification root containing the frozen Coverage/Limits files; the partial bundle then uses the existing closeout consumer unchanged. Absolute machine-local command files and raw logs remain under ignored `Build/Validation/030/` or the clean checkout’s corresponding directory. No private command paths are promoted. Later evidence-only commits retain tested software `9154e421eaaf5a01755d80736af322a1caab9215`.

## Windows preparation — 2026-09-16, blocked before visible execution

The Windows checkout was initially on an unrelated clean 029 branch. That checkout was preserved; separate evidence and detached `9154e421` software worktrees were created. The current hardware inventory identifies a Lenovo physical workstation with an NVIDIA GeForce RTX 3080 discrete Vulkan adapter, driver 581.32, and local fixed NTFS staging. Vulkan advertises `VK_EXT_swapchain_maintenance1` with its feature enabled in the capability inventory; an actual lab retirement-path selection has not yet been observed.

The active Console session could not expose an unlocked input desktop, including a check outside the tool sandbox. The session also contained LogonUI. The maintainer was asked to unlock the desktop. This is an environment blocker before T115, not a failed GPU run or an optional-extension blocker. No machine run, frame/readback/retirement result, formal capture or human acceptance is claimed.

Strict build, focused test and tooling outcomes, binary/log digests, fresh cooked generations, and the original session checks are recorded in [the bounded checkpoint](Windows-Vulkan/9154e421-20260916-preflight/checkpoint.json). Both Lantern and Sponza were freshly cooked with the Windows target and passed `validate --strict-files`. The five fixed machine command files are prepared, including exactly 180 Sponza lifecycle steps, 61 Sponza mode/rejection steps and 12 Lantern integration steps. The inherited 512x512 formal capture and current human launch commands are prepared only; no linked human request can be issued before the corresponding native machine case passes.

Windows automatic line-ending conversion was removed from the isolated worktrees by restoring exact committed bytes. All 82 frozen policy identities match, and no source/validation-policy edit was made. The current 64 original hosted/M4 artifacts were digest-checked and reconstructed at their original relative paths. The independent closeout consumer revalidated all 13 hosted and 14 M4 machine records and returned the same 21 missing gates; its original output is retained beside the checkpoint. This recheck does not close Windows T118 or any formal/human gate.

Reviewed tasks remain 116/127. Resume after an active unlocked Console or supported RDP session is available: recheck session and frozen inputs, run/verify T115 then the three Windows lifecycle cases, re-evaluate full T118, then capture/compare formal SDR and collect actual maintainer navigation decisions. Keep new output names and retain the `IdleAssumed` qualification when the forced AcquireHistory smoke passes. Exact local commands and raw logs remain under the frozen checkout's ignored `Build/Validation/030/local-20260916/`; `LOCAL-HANDOFF.md` and `run_cases.py` provide the prepared continuation. No 028/029 exception is used.

## Windows execution — 2026-09-16, T115 passed; T118 blocked

The maintainer unlocked the same Console session; before/after checks show the Default input desktop. The exact frozen software `9154e421eaaf5a01755d80736af322a1caab9215` executed on NVIDIA GeForce RTX 3080. All five cases used fresh command/native/wrapper paths and the repository `interactive_lab_validation.py run`; independent `verify` processes consumed original bytes. The machine matrix was not expanded.

| Case | Present-queued / submitted | Script completion | Result / shutdown |
| --- | ---: | ---: | --- |
| Sponza SDR endurance | 1120 / 1120 | none | passed, PresentationFence / Proven |
| Lantern forced fallback smoke | 120 / 120 | none | passed, AcquireHistory / IdleAssumed |
| Sponza SDR lifecycle stress | 1120 / 1158 | 180/180 | passed, PresentationFence / Proven |
| Sponza SDR mode stress | 0 / 0 | 0/61 | failed on BT709 profile request, exit 5 |
| Lantern lifecycle integration | 31 / 33 | 9/12 | failed on BT709 profile request, exit 5 |

Endurance accounts for exactly 120 warmup + 1000 measured presentations; smoke has 120 with no excluded warmup. Every report has zero capture-disabled readback copies/maps/waits, live queue/device idles and final native owners. Failure cleanup is Proven, but the two cases remain failed. The forced fallback has 117 proven releases, 3 pre-cleanup presentation owners and one successful terminal idle; its zero final owners do not prove all presentation releases or physical scanout. Preserve the original wrapper's explicit compatibility limitation.

[Windows SDR evidence](../SDR/windows-vulkan.json) closes T115. [Lifecycle evidence](../UI/physical-lifecycle.json) now contains the four passing M4 cases, one passing Windows case and two failed Windows cases. [Failure analysis](Windows-Vulkan/9154e421-20260916/failure-analysis.json) records actual surface inventory and frozen source locations: outgoing Vulkan conversion maps sRGB/BT709/pass-through to SRGB_NONLINEAR_KHR, while reverse enumeration exposes only SrgbNonlinear and the lab requires an exact engine format/color-space pair. The fixed BT709 transition is consequently rejected before rendering. Reconcile profile semantics, capability advertisement and required coverage; do not blindly advertise an unsupported native color space, remove the required transition, or weaken acceptance. No software/policy fix was made; a correction requires a new freeze and coordinated affected hosted/M4/Windows validation.

[Independent consumer results](Windows-Vulkan/9154e421-20260916/consumer-results.json) pass the three successful Windows wrappers and reject both failed wrappers. The [full attempted bundle](windows-attempt-bundle.json) includes all 19 machine cases and fails closeout. The [passing-only partial bundle](partial-bundle.json) verifies 17 machine and 13 hosted records and produces the updated [incomplete aggregate](aggregate.json): two failed machine gates, four unexecuted formal SDR gates and twelve missing human decisions remain. Failed original evidence is retained separately and is never passed or omitted from the attempted bundle.

Two current Windows navigation requests generated by the successful endurance/smoke cases are added to the [request index](../UI/human-requests.json), bringing prepared requests to twelve and decisions to zero. [Review instructions](../UI/Windows-Vulkan/9154e421-20260916/human-review-instructions.json) identify the required operations/observations without writing acceptance. T119 formal capture was not started because full T118 has not passed; no Candidate, image difference, tolerance change or Accepted promotion is claimed. Reviewed count is **117/127**. Raw argv, logs, binaries, strict cooked generations and future launch inputs remain under the frozen checkout's ignored `Build/Validation/030/local-20260916/`. Both checkout SHA guards still pass; the evidence-only commit retains the tested software revision.
