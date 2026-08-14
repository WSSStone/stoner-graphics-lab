# Feature 025 Asset Cooker Fixtures

This tree owns deterministic source, mutation, corruption, concurrency, and
scale inputs for the offline Asset Cooker. Fixtures are repository-relative,
license-audited, and recorded in `Validation/025/fixture-manifest.json` before
they are accepted by a test or validation gate.

## Layout

- `Representative/`: valid Feature 021-024 source assets and expected roots.
- `Mutation/`: addition, edit, removal, rename, dependency, producer, schema,
  and target-profile invalidation inputs.
- `CorruptCache/`: at least 15 independent DDC corruption cases.
- `CorruptPublished/`: at least 30 independent published-generation cases.
- `Concurrency/`: same-key and same-publication-root subprocess fixtures.
- `Contracts/`: target profile, derived-key, envelope, manifest, and report
  golden or malformed data.
- `Cli/` and `Reports/`: argument, exit-category, normalized report, and
  diagnostic goldens.
- `Scale/`: generated 1,000-asset/5,000-edge corpus metadata.

## Authoring Rules

1. Every checked-in file has a canonical repository-relative path, SHA-256,
   provenance URI, license, expected validity/result, represented asset type,
   and applicable target profile in the fixture manifest.
2. Generated corruptions identify their immutable base fixture, mutation name,
   and generator revision. They never overwrite the valid base.
3. Host paths, timestamps, process IDs, random seeds, and enumeration order do
   not enter expected normalized artifacts.
4. Large generated DDC and cooked trees stay under ignored `Saved/` paths or CI
   artifacts. Only compact source fixtures and normalized evidence are tracked.
5. Fixture changes and manifest changes land in the same commit.
