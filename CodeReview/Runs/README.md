# Review Run Workspace

`crctl` creates active review state in this directory. Run snapshots contain
machine state, handoffs, batch journals, findings, traceability, and evidence
used to resume a long audit.

Run directories are intentionally ignored on the development mainline. On a
review branch, explicitly stage only recovery-critical checkpoints with
`git add -f`. Keep raw logs, screenshots, build products, and generated output
untracked or publish them as CI artifacts.

When a review closes, publish a self-contained report under
`doc/code-reviews/` and remove its run snapshot from the branch's final file
tree. The merge commit preserves the detailed checkpoint history.
