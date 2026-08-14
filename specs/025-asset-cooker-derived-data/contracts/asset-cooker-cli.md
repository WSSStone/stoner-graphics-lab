# Contract: StonerAssetCooker CLI

## Executable

```text
Build/<Platform>/<Config>/Tools/AssetCooker/StonerAssetCooker[.exe]
```

The executable links only the offline AssetCooker tool library, Asset, and Core.
It never loads Application, Renderer, RHI, or a graphics Backend.

## Common Parsing Rules

- UTF-8 arguments after platform command-line conversion.
- Long options only; option names are case-sensitive kebab-case.
- An option occurs once unless explicitly repeatable.
- Unknown options, missing values, duplicate singleton options, and positional
  arguments fail as `InvalidArguments` before filesystem mutation.
- Paths are accepted as entered for access but normalized absolute host paths
  never enter deterministic reports, manifests, keys, or generation identity.
- Source scopes, output root, and DDC root must be canonically pairwise
  non-overlapping. Equivalent source-scope aliases collapse to one scope;
  absent, out-of-scope, or mutually overlapping scopes fail before mutation.
- An explicit report path must not alias source content, DDC entries,
  generation payloads/manifests, or `Current.json`.
- `--report` is optional. stdout receives a concise human summary; stderr
  receives a concise failure summary. `--normalized-report` omits host telemetry.
- No option executes a shell command or accepts executable code.

## `cook`

```text
StonerAssetCooker cook
  --source-root <path> [--source-root <path> ...]
  (--root <canonical-asset-id> [--root ...] | --cook-all)
  --target-profile <profile.json>
  --output <path>
  --ddc <path>
  [--clean]
  [--workers <1..32>]
  [--lease-timeout-ms <0..600000>]
  [--report <path>]
  [--normalized-report]
```

- At least one source root is required.
- `--root` and `--cook-all` are mutually exclusive.
- Explicit-root mode requires at least one root. Cook-all that discovers no
  supported typed output fails as `DiscoveryFailure`; empty generations are not
  published.
- Default mode is incremental. `--clean` ignores existing DDC hits for node
  execution but does not delete or overwrite valid entries.
- Ordinary cook quarantines invalid DDC entries and rebuilds them.
- Success means a complete generation validates and `Current.json` references
  it. Publishing an already current equivalent generation is success.
- Once atomic replacement reports success, `cook` returns success. A subsequent
  audit read failure is emitted as a stable non-fatal diagnostic because the
  committed pointer cannot truthfully be reported as rolled back.

## `plan`

```text
StonerAssetCooker plan
  --source-root <path> [--source-root <path> ...]
  (--root <canonical-asset-id> [--root ...] | --cook-all)
  --target-profile <profile.json>
  --output <path>
  --ddc <path>
  [--workers <1..32>]
  [--report <path>]
  [--normalized-report]
```

Plan performs bounded discovery/import, root selection, graph validation,
derived-key calculation, and read-only DDC hit/miss validation. It writes no DDC
entry, quarantine record, staging generation, pointer, or source file. The
explicitly requested report is its only permitted output. Corrupt entries are
reported as expected rebuilds but are not moved.

## `validate`

```text
StonerAssetCooker validate
  --output <path>
  [--generation <64-lowercase-hex>]
  [--strict-files]
  [--report <path>]
  [--normalized-report]
```

Without `--generation`, validation reads and validates `Current.json`, then the
referenced generation. With an explicit generation, it validates that immutable
directory without changing current state. It requires no source root, importer,
or DDC. `--strict-files` reports and fails on unexpected files within the
selected generation, except contract-declared metadata files.

Validation never repairs, removes, or rewrites published bytes.

## `validate-cache`

```text
StonerAssetCooker validate-cache
  --ddc <path>
  [--key <64-lowercase-hex>]
  [--max-errors <1..4096>]
  [--report <path>]
  [--normalized-report]
```

This is explicit strict cache validation. It validates one key or bounded stable
enumeration of all entries. Every invalid entry is reported and causes failure;
no entry is rebuilt. Invalid entries may be moved to Quarantine only when the
validator can preserve complete failure evidence atomically; the disposition is
reported. Missing `--key` entries are `CacheFailure`.

## `inspect`

```text
StonerAssetCooker inspect
  (--output <path> [--generation <digest>] |
   --ddc <path> --key <digest> |
   --target-profile <profile.json>)
  [--report <path>]
  [--normalized-report]
```

Exactly one subject is accepted. Inspect parses and normalizes evidence without
modification. Profile inspection prints display metadata, canonical effective
configuration, effective digest, and known cooker projections.

## Exit Codes

| Code | Category | Meaning |
|---:|---|---|
| 0 | Success | Requested operation completed |
| 2 | InvalidArguments | CLI/request contract invalid |
| 3 | InvalidProfile | Target profile/schema/capability invalid |
| 4 | DiscoveryFailure | Scope, enumeration, resolution, or import catalog failed |
| 5 | GraphFailure | Identity, dependency, cycle, closure, or graph limit failed |
| 6 | CookFailure | Type-specific cooking or payload encoding failed |
| 7 | CacheFailure | Strict cache validation or unrecoverable DDC operation failed |
| 8 | SourceChanged | Snapshotted input changed before publication |
| 9 | LeaseTimeout | Publication lease unavailable before timeout |
| 10 | PublishedValidationFailure | Current pointer, manifest, or published payload invalid |
| 11 | PublicationFailure | Staging, install, or current-pointer commit failed |
| 12 | IoFailure | Portable filesystem operation failed |
| 13 | InternalFailure | Invariant or unexpected failure; no partial publication |

## Stable Diagnostic Subject

Each failure report includes, where available:

```text
category, stage, assetId, dependencyChain, sourceLocator,
targetProfileDigest, derivedKey, generationId, field, reason
```

Absolute temporary paths, native error message text, pointer values, process
IDs, thread IDs, timestamps, and completion order are excluded from normalized
diagnostics. Native numeric error codes may appear only under host telemetry.
