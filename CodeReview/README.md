# Code Review Program

This directory contains the reusable whole-project audit process. Reviews are
independent of Speckit feature numbering and do not consume roadmap feature IDs.

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

## Resume

Run `crctl recover` before reading chat history. It reports the authoritative
state, Git drift, unrecorded changes, and the next permitted command. The
run-local `handoff.md` is the human-readable recovery document.

## Close

`crctl close` refuses to close a review with open accepted findings, incomplete
traceability, failed required gates, or unfinished batches. Keep batch commits
and merge the review PR with a merge commit.

See [PROCESS.md](PROCESS.md) for the protocol and
[Tools/README.md](Tools/README.md) for CLI and environment details.
