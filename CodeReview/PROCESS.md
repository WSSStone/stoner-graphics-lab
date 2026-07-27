# Whole-Project Review Process

## Authority And Scope

When artifacts disagree, use this authority order:

1. Constitution
2. Clarified feature specification requirements and success criteria
3. Contracts and plan
4. Roadmap
5. Tasks
6. Code and tests

Historical specifications must not be rewritten merely to conceal an
implementation defect. Controlled public API corrections are allowed only when
callers, tests, affected specifications, design documentation, and migration
notes are updated in the same fix cluster.

## Session Limits

One Codex session executes exactly one step returned by `crctl next`.

- Each implementation batch is divided into responsibility-domain
  `Inspect -> Fix -> Verify` triplets; a batch-wide three-step packet is invalid
  when its scope exceeds an inspection limit.
- Inspection: at most one responsibility domain, eight production files, or
  1,500 production lines, whichever limit is reached first.
- Fix: at most three tightly related findings or one public API migration.
- Every step records files read, findings, evidence, gates, commit, and next
  command in machine state and generated handoff documents.
- Interrupted work resumes with `crctl recover`. Unrecorded changes block
  progress until classified.

## Severity

| Level | Meaning | Policy |
|---|---|---|
| S0 Blocker | Data loss, security, guaranteed crash, or review gate invalidation | Fix immediately |
| S1 Critical | High-probability correctness, lifecycle, portability, or ownership failure | Fix in this review |
| S2 Major | Material architecture, maintainability, testing, or performance defect | Fix in this review |
| S3 Minor | Local quality issue with limited behavioral risk | Fix only when low-risk; otherwise defer |

Finding states are:

`Open -> Triaged -> Accepted|Deferred|Rejected -> Fixed -> Verified`

Every deferred finding names a future roadmap phase or an explicit debt owner.

## Git Protocol

- Use one umbrella review branch and one independent worktree.
- The first commit contains only the framework, environment, templates, tools,
  and charter. Open a Draft PR immediately afterward.
- Use one conventional commit per related fix cluster.
- Push and run CI at each batch boundary.
- After the Draft PR exists, do not rebase or force-push.
- Merge upstream `master` only at batch boundaries, then rebuild CodeGraph and
  rerun affected gates.
- Treat `CodeReview/Runs/<id>-<slug>/` as branch-local execution state. Force-add
  only the checkpoints needed for durable recovery; raw output remains ignored.
- At closeout, archive the durable result under `doc/code-reviews/` and remove
  the run snapshot from the final tree.
- Default to Squash Merge so the development mainline receives the reviewed net
  code changes, reusable CR infrastructure, and final report without importing
  checkpoint commits or deleted run data into its history.
- Use the PR and review branch as the execution-time audit trail. The final
  report is the durable mainline authority for findings, decisions, gates, and
  deferred debt.
- Use a merge commit only when an explicit regulatory, contractual, or forensic
  requirement demands permanent per-commit history.

## Evidence

Commit summaries, hashes, versions, structured findings, decisions, and compact
reports. Large raw logs belong in ignored `Evidence/output/` directories or CI
artifacts. Every accepted finding identifies a requirement, code location,
impact, repair, verification, and commit.

The final report records the frozen baseline and audited head, scope, finding
counts, verified gates, material decisions, and deferred debt. Step state,
handoffs, batch journals, and raw evidence are execution-time records in the
review branch, PR, or external artifacts; they are not permanent mainline
content unless a stronger audit policy explicitly requires it.

## Completion

A review closes only when traceability is complete, all accepted S0-S2 findings
are verified, S3 findings are fixed or deferred, required local and remote gates
pass, CodeGraph is rebuilt at final HEAD, the Python environment is reproducible,
and a future review can be initialized in a temporary directory.
