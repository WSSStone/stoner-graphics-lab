# Feature 026 Runtime Asset Manager Fixtures

This tree owns deterministic source/cooked equivalence, dependency, mutation,
malformed-generation, and scale inputs for the Runtime Asset Manager. Compact
fixtures are checked in; generated publications and caches stay under ignored
Feature 026 work roots.

## Layout

- `Equivalence/`: matching development source and cooked payload cases for all
  Feature 021-024 payload families.
- `Dependencies/`: required closures, optional fallback declarations, cycles,
  missing nodes, and type/version conflicts.
- `Mutation/`: one immutable base plus mutations at each pre-publication stage.
- `Cooked/`: valid and malformed generation indexes, payload envelopes,
  required/optional extension declarations, and read-only publication layouts.
- `Scale/`: deterministic generated 1,000-asset/5,000-edge graph descriptions.

## Authoring Rules

1. Every fixture has a canonical repository-relative identity, expected mode,
   expected stable result category, and SHA-256 provenance entry.
2. Equivalent source and cooked cases preserve AssetId, type, dependency roles,
   source/version evidence, target meaning, and normalized payload semantics.
3. Mutations never overwrite their valid base and identify the exact lifecycle
   boundary where the mutation becomes visible.
4. Optional dependency success names the owning payload validator and concrete
   satisfiable fallback. Soft dependency role alone is not fallback evidence.
5. Malformed cases change one contract dimension at a time. They must not rely
   on host paths, timestamps, process IDs, thread scheduling, or random input.
6. Scale inputs use fixed graph-generation revisions and seeds. Generated
   payload trees remain under `Saved/Feature026*` or CI artifacts.
