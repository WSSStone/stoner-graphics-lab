# B06-S09 Evidence: Shader Library Variant Metadata Verification

Finding: `CR001-B06-F002`.

Fix commit: `0daa334`.

Verification summary:

- Parent `0daa334^` sorted shader variants and inserted records without
  registration-time internal consistency checks for variant metadata.
- Current HEAD rejects duplicate/empty allowed flags, empty/duplicate variant
  ids, duplicate canonical variant keys, and undeclared variant flags before
  insertion.
- Regression tests for duplicate allowed flags, undeclared variant flags,
  duplicate variant ids, and duplicate variant keys are present and pass.
- `fallback-strict`, `strict-release`, and `sanitizers` gates all pass after the
  fix.

Decision: mark `CR001-B06-F002` Verified.
