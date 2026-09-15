# Feature 030 closeout evidence

T105–T112 implementation/preflight is complete; hosted, frozen physical and current human acceptance remain pending. `preflight.json` records working-tree checks, not final authority. Raw logs and preliminary native reports remain under ignored `Build/Validation/030`.

The thin run/verify/closeout tool reuses inherited 029 provenance and image checks. The fixed hosted matrix has thirteen required job records; it does not satisfy the nineteen physical machine cases, four formal SDR gates or twelve human gates. Intel macOS is explicitly skipped, never passed.

Follow `specs/030-interactive-rendering-lab/hardware-validation.md` for exact command files, lifecycle recipes, strict cooking, session provenance and human linkage. A missing or failed gate leaves closeout open. Final software freeze and hosted run identities will be recorded after source commit.

## Current frozen software

Software `9357028a62a74868b47847d413163cc3d2130d82` is frozen in the clean checkout `Build/Worktrees/030-frozen-97c09338`. Run [34983045938](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34983045938) is in progress. Earlier attempts retain their actual outcomes below; no final hosted aggregate or physical/human authority is claimed. Push and rerun authorization is already granted.

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
