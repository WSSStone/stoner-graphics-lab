# Material/Shader Fixture Matrix

Feature 023 owns deterministic source-definition fixtures here.

## Valid Corpus

- 12 ShaderProgram definitions: six repository programs, source-only,
  compute, permutation, interface, alternate-profile, and shared-dependency
  cases.
- 12 Material definitions spanning every domain, blend mode, render-state
  boolean, scalar/vector/color/texture parameter, and permutation category.
- 16 MaterialInstance definitions spanning Material and MaterialInstance
  parents, root-to-leaf overrides, exact depth, unresolved soft references,
  and deterministic insertion orders.
- Ten semantically equivalent insertion orders normalize to identical bytes.

## Invalid Corpus

At least 40 checked-in mutations cover envelope/schema fields, decoded
duplicate keys, malformed UTF-8/JSON, versions/extensions, numeric and text
limits, every bounded collection, dependency identity/locator/digest mismatch,
SPIR-V structure/stage/entry mismatch, permutations/interfaces, material
parameters, instance chains, truncation, and transactional rollback.

## Golden Evidence

`Golden/canonical-digests.txt` stores the canonical UTF-8/LF SHA-256 for every
valid definition. `Golden/failure-results.txt` stores the first stable
`EAssetResult` category for every invalid definition, and
`Golden/inventory.json` records the generated corpus inventory. The canonical
definition does not embed its own digest. External dependency digests always
refer to exact checked-in GLSL/SPIR-V bytes.

The C++ loader is the authority that regenerates canonical and failure golden
evidence. The fixture generator deliberately preserves those files so a Python
fixture refresh cannot silently bless a parser or canonical-writer change.

## Limit Provenance

The checked-in malformed corpus covers representative syntax, schema,
extension, type, numeric, duplicate-key, and truncation failures. Dynamic C++
tests additionally set limits to the exact observed value and one below it for
definition bytes, shader stages/payloads, material parameters/dependencies, and
relative locators. They also exercise zero-sentinel rejection and checked
aggregate addition/multiplication. Schema arrays are rejected in the JSON
preflight before typed model allocation.

## Repository Integration

The synthetic corpus is complemented by all six definitions under
`Content/Shaders`: Triangle plus Deferred Surface, Composition,
DirectionalLight, PointLight, and SpotLight. Together they own 11 GLSL source
dependencies and 11 SPIR-V payload dependencies. Point and Spot intentionally
retain distinct typed identities even where their vertex payload bytes are
identical.

Regenerate the checked-in corpus with:

```sh
python3 Tests/Fixtures/MaterialShader/generate_fixtures.py
```

After changing parser or schema behavior, run the focused C++ suite to compare
the regenerated canonical and failure facts against the checked-in golden
files. Review any digest or first-failure change before updating evidence.
