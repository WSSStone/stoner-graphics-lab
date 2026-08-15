# Contract: Runtime Loading Strategies

## Common Strategy Boundary

The manager selects exactly one private strategy for its lifetime. A strategy
receives one load key plus cancellation/limit context and returns immutable
scratch metadata/payload outputs and bounded diagnostics. It does not own request
slots, coalescing, dependency scheduling, cache retention, or callbacks.

Cancellation/limit context contains a cooperative token and monotonic deadline.
Runtime-compatible resolver/importer/loader adapters must check it at bounded
work boundaries and return by that deadline. Legacy extensions that cannot make
that guarantee are rejected for runtime-manager registration or use.

Both strategies must produce the same canonical AssetId, type, dependency roles,
version evidence, target meaning, and normalized payload semantics.

For an optional edge, the strategy also returns a normalized fallback decision
only after the owning payload type's existing validator confirms a concrete,
satisfiable fallback in that immutable payload. The manager never infers
fallback from `Soft` strength alone. Source and cooked paths must agree on target,
role, fallback token, validation outcome, and stable reason.

## Development Source Strategy

1. Resolve through registered public resolver contracts.
2. Hold bounded authoritative source lease and capture version evidence.
3. Probe/import through deterministic 020 dispatch.
4. Validate all emitted typed outputs atomically; locate requested identity.
5. Normalize metadata and discover the required dependency closure.
   A failed optional dependency is tolerated only when the owning payload
   declares a satisfiable fallback; undeclared/unsatisfied fallback fails root.
6. Before publication, re-resolve every source-manifest record and compare
   complete size/digest/version evidence.
7. Publish all outputs to the operation context atomically or none.

Changed/missing evidence at step 6 returns `SourceChanged`. There is no retry,
watcher, lock, cache mutation, or hot reload. A later request after unload starts
from current source state.

## Strict Cooked Strategy

### Startup

1. Parse bounded `Current.json` to select a generation.
2. Acquire shared generation lease.
3. Validate that exact generation using `IndexAndLayout` policy.
4. Verify target profile evidence and required extensions.
5. Build a unique sorted AssetId record index and publish the bound generation.

Any failure releases the lease and creates no usable manager. Pointer changes
after binding are ignored.

### Request

1. Find exact AssetId/type record in the bound index.
2. Validate target and dependency declarations.
3. Construct contained manifest-listed payload path only.
4. Bounded-read exactly the declared size.
5. Verify envelope digest and decode through `FAssetCookContractCodec`.
6. Verify identity, type, codec revision, payload schema, target, and dependency
   agreement with the manifest record.
7. Return immutable scratch payload for scheduler publication.

No resolver, importer, source root, DDC, or Tools fallback is reachable.
Unsupported codec/schema/extension/target fails closed.
Required extensions must have one compatible active registration; unsupported
optional extensions are ignored only when their manifest contract declares them
non-semantic. Both decisions are present in inspection evidence.

## Published Validator Policy

- `FullPayloads` remains the default and preserves all Feature 025 behavior.
- `IndexAndLayout` performs every pointer/manifest/generation/record/path/layout
  check but does not read or decode payload bodies.
- Per-request strict loading supplies the omitted body validation before any
  runtime publication.
