# B02 Value Identity Local Evidence

- Commit under test: `0bfcdec`
- Platform: macOS Apple Silicon
- Strict build: PASS (`scons config=debug strict=1`)
- Core foundation result: 60 passed, 0 failed
- Deterministic full-suite command:
  `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1 Build/Mac/Debug/Tests/StonerTest`
- Deterministic full-suite exit code: 0
- Focused probe output: `1 1 1`
  - moved destination retains original identity
  - moved source equals a name reconstructed from its current text
  - forced same-hash/different-text comparison remains unequal
- CR tool tests: 19 passed
- Diff whitespace check: PASS

An ordinary unisolated test run returned 1 only for the already accepted
`CR001-B08-F001` native deferred checks:

- real Vulkan submission
- mapped attachment semantic tolerances
- final deferred native validation result

Core foundation remained 60/0 in that run.

