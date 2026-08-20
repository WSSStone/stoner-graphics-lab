# Metal Shader Fixtures

This directory owns cross-host derivation cases, normalized MSL expectations,
binding-map evidence, malformed records, and strict-cooked selection cases.
GLSL/SPIR-V remains authoritative. MSL is deterministic evidence; native Metal
libraries are generated outputs tied to an exact Apple toolchain tuple.

`derivation-cases.json` is portable across Windows, macOS, and Linux.
`native-evidence-golden.json` defines fields and comparison scope rather than a
machine-specific metallib digest. `malformed-payloads.json` and
`strict-generation-cases.json` are the fail-closed runtime/publication corpus.
