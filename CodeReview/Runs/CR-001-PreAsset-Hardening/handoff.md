# CR-001 Handoff

- Base: `9092a97593fb29cffbffdbe534e3dda143f463a5`
- Current HEAD: `9092a97593fb29cffbffdbe534e3dda143f463a5`
- Branch: `codex/review-001-pre-asset-hardening`
- Worktree: `/Users/wangshi/Documents/UGit/stoner-graphics-lab-cr-001`
- Active batch/step: `B00` / `B00-S01`
- Working tree dirty: True
- Open findings: none
- Latest gates: {}

## Recovery

Run `conda run -n stoner-cr python CodeReview/Tools/crctl.py recover --id CR-001`.
Do not advance when recover reports an unrecorded diff.

## Next Command

`conda run -n stoner-cr python CodeReview/Tools/crctl.py next --id CR-001`
