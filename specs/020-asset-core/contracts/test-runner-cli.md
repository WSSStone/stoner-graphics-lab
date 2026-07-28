# Contract: StonerTest Suite Selection

**Feature**: 020-asset-core
**Debt closed**: `CR001-B09-F003`

## Commands

```text
StonerTest
StonerTest --list-suites
StonerTest --suite <name> [--suite <name> ...]
StonerTest --suite all
```

## Behavior

### No Arguments

Runs every registered suite in canonical registry order. This preserves current
CI and local behavior. The real executable's no-argument behavior is verified
by an external CI or shell invocation, not by recursively launching the
no-argument executable from a suite that it would execute.

### List Suites

`--list-suites` prints one canonical suite name per line and exits `0` without
running tests. The list includes `asset`.

### Select Suites

- Each `--suite` requires one following suite name.
- Multiple selections are allowed.
- Duplicate selections run once.
- Selected suites run in canonical registry order, not argument order.
- `all` selects every suite and cannot be combined with a different semantic
  result.
- `--suite asset` runs only `RunAssetCoreTests`.

### Existing Child Probes

The logging fatal/assertion child arguments remain exact single-purpose modes
and are recognized before general suite parsing. They do not appear in
`--list-suites`.

## Exit Codes

| Code | Meaning |
|---|---|
| `0` | Requested suites completed with zero test failures, or list succeeded |
| `1` | At least one selected suite reported a test failure |
| `2` | Unknown argument, unknown suite, missing suite name, or invalid combination |

## Diagnostics

Invalid invocation writes deterministic usage and the offending argument or
suite to standard error. It does not run any suite.

## Compatibility

- Existing no-argument callers require no changes.
- Suite selection must not use environment-variable skips or output filtering.
- Adding future suites requires one table registration, not another parser
  branch or new executable.

## Test-Only Failure Injection

The production CLI has no failure-injection option. Unit tests exercise exit
status propagation by constructing the reusable suite registry in process,
registering a fake callback under the canonical `asset` suite name, selecting
only `asset`, and making that callback report one failure. The test verifies
that no unselected callback runs and the registry returns `1`.

This seam is available only to test code through the suite table API. It does
not use an environment variable, output filtering, a hidden production command
line argument, or the real `RunAssetCoreTests` implementation.

Executable-level integration tests may launch `--list-suites`, `--suite asset`,
and malformed invocations because those modes do not select the test-runner
contract suite. They MUST NOT launch a no-argument child from inside
`StonerTest`; T046/T051-style external invocations verify the all-suite default
without recursion.
