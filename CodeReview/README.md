# Code Review Program

This directory contains the reusable whole-project audit process. Reviews are
independent of Speckit feature numbering and do not consume roadmap feature IDs.
Reusable tools, templates, and process documentation live on the development
mainline. Per-review execution state under `Runs/` is temporary branch-local
working data; a closed review contributes a compact report under
`doc/code-reviews/` instead.

## Start A Review

1. Create a `codex/review-*` branch in an independent worktree.
2. Create the pinned Python environment:

   ```sh
   conda env create -f CodeReview/Tools/environment.yml
   ```

3. Initialize and inspect the next step:

   ```sh
   conda run -n stoner-cr python CodeReview/Tools/crctl.py init \
     --id CR-002 --slug Example-Audit --baseline "$(git rev-parse HEAD)"
   conda run -n stoner-cr python CodeReview/Tools/crctl.py doctor
   conda run -n stoner-cr python CodeReview/Tools/crctl.py next
   ```

4. Execute one issued step packet per Codex session. End every step with
   `complete`, `render`, `lint`, evidence, and a conventional commit.

`CodeReview/Runs/*` is ignored on the development mainline. A review branch that
needs durable cross-machine checkpoints may explicitly stage selected run files
with `git add -f`. Do not force-add raw build logs, screenshots, or generated
output.

## Resume

Run `crctl recover` before reading chat history. It reports the authoritative
state, Git drift, unrecorded changes, and the next permitted command. The
run-local `handoff.md` is the human-readable recovery document.

## Close

`crctl close` refuses to close a review with open accepted findings, incomplete
traceability, failed required gates, or unfinished batches. After close:

1. Write a self-contained final report under `doc/code-reviews/`.
2. Remove the completed run snapshot from the branch's final file tree.
3. Squash Merge the PR by default so the development mainline receives one
   coherent review change without checkpoint commits or deleted run data.

The PR and review branch carry detailed process history while the review is
active; the final report carries the durable conclusion. Use a merge commit only
when an explicit regulatory, contractual, or forensic requirement demands
permanent per-commit history.

See [PROCESS.md](PROCESS.md) for the protocol and
[Tools/README.md](Tools/README.md) for CLI and environment details.
