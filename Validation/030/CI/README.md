# Feature 030 closeout evidence

T105–T112 implementation/preflight is complete; hosted, frozen physical and current human acceptance remain pending. `preflight.json` records working-tree checks, not final authority. Raw logs and preliminary native reports remain under ignored `Build/Validation/030`.

The thin run/verify/closeout tool reuses inherited 029 provenance and image checks. The fixed hosted matrix has thirteen required job records; it does not satisfy the nineteen physical machine cases, four formal SDR gates or twelve human gates. Intel macOS is explicitly skipped, never passed.

Follow `specs/030-interactive-rendering-lab/hardware-validation.md` for exact command files, lifecycle recipes, strict cooking, session provenance and human linkage. A missing or failed gate leaves closeout open. Final software freeze and hosted run identities will be recorded after source commit.

## Frozen software and current blocker

Software: `31519b15dc44fea61151eac3ce6345cf5a340129`. `software-freeze.json` records policy identities; a clean detached checkout is prepared at `Build/Worktrees/030-frozen-31519b15`. The preflight remains working-tree evidence and is not relabeled.

T114 is blocked: automatic approval review rejected pushing the committed contents to `git@github.com:WSSStone/stoner-graphics-lab.git` without explicit destination authorization. No push, workflow dispatch or hosted pass occurred. After approval, push the frozen source commit with `git push origin 31519b15dc44fea61151eac3ce6345cf5a340129:refs/heads/030-interactive-rendering-lab` and verify all thirteen jobs at that revision; evidence-only commits must not replace the named tested revision. T115 onward and current human decisions remain pending.

## Hosted attempt 1

The maintainer authorized execution. Commit `31519b15` was pushed and run [34963601214](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34963601214) started at that exact SHA. Linux failed because GLFW 3.3 has no `GLFW_SCALE_FRAMEBUFFER`; Windows Debug failed on concurrent compiler writes to `vc140.pdb`. The GLFW hint is now version-guarded; Windows restores the established serial build command. The original freeze is invalidated and affected validation will run again.

## Hosted attempt 2

Run [34963871700](https://github.com/WSSStone/stoner-graphics-lab/actions/runs/34963871700) at `97c09338` passed the first compilation blockers, then found GCC temporary-validator span warnings and an obsolete Asset boundary rule requiring direct yyjson compilation. Named validator lifetimes, shared-private-library verification, six boundary tests and an always-run boundary stamp address these findings. No prior result is promoted to final acceptance.
