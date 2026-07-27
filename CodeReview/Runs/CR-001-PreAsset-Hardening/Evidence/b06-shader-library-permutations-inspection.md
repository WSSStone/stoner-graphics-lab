# B06-S07 Evidence: Shader Library And Permutations Inspection

Step: `B06-S07`.

Inspected shader library/permutation/material binding/resource requirement code
against Feature 014 contracts.

Key evidence:

- `FShaderPermutation::SetFlags` canonicalizes request flags.
- `FShaderLibrary::ResolveVariant` validates request flags against registered
  allowed flags.
- `FShaderLibrary::RegisterShaderRecord` does not reject duplicate/empty variant
  metadata or variant permutations using undeclared flags.
- Tests cover resolve-time unknown flags and missing variants but not invalid
  shader records at registration time.

Finding:

- `CR001-B06-F002`: Shader library registration accepts invalid variant
  metadata. Severity S2, Accepted.

No production or test source changed in this inspection step.
