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
- Preserve the audit chain with a merge commit; do not squash.

## Evidence

Commit summaries, hashes, versions, structured findings, decisions, and compact
reports. Large raw logs belong in ignored `Evidence/output/` directories or CI
artifacts. Every accepted finding identifies a requirement, code location,
impact, repair, verification, and commit.

## Completion

A review closes only when traceability is complete, all accepted S0-S2 findings
are verified, S3 findings are fixed or deferred, required local and remote gates
pass, CodeGraph is rebuilt at final HEAD, the Python environment is reproducible,
and a future review can be initialized in a temporary directory.
