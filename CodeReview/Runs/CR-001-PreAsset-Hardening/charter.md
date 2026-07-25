# CR-001: PreAsset-Hardening

## Objective

Audit and harden Features 003-019 before introducing the Asset layer in Feature
020. This review does not implement Feature 020.

## Frozen Baseline

- Commit: `9092a97593fb29cffbffdbe534e3dda143f463a5`
- Branch: `codex/review-001-pre-asset-hardening`
- Scope: project C++ sources, tests, SCons, CI, demo, validation, roadmap,
  constitution, and Features 003-019 artifacts.

## Policy

- Authority: constitution, clarified spec FR/SC, contracts/plan, roadmap, tasks,
  then code/tests.
- Fix every accepted S0-S2 finding.
- Fix low-risk local S3 findings and defer the rest with an explicit target.
- Controlled public API repair is allowed only with callers, tests, documents,
  and migration notes updated together.
- Do not rewrite historical specifications to hide defects.

## Completion

All requirements are classified with evidence; accepted findings are verified;
required local and three-platform remote gates pass; CodeGraph covers final
HEAD; the `stoner-cr` environment and CLI tests reproduce cleanly; and a
temporary CR-002 initialization succeeds.
