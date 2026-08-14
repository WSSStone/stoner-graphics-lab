# Feature Specification: Asset Cooker, Manifest & Derived Data

**Feature Branch**: `025-asset-cooker-derived-data`  
**Created**: 2026-08-14  
**Status**: Draft  
**Input**: User description: "为 roadmap 下一项制定 spec"

## Clarifications

### Session 2026-08-14

- Q: 如果源文件或依赖在一次 cook 尚未完成时发生变化，本次 cook 应采用什么一致性规则？ → A: 规划阶段记录完整输入版本，处理及发布前再次核验；任一输入发生变化则本轮失败且不发布。
- Q: 用户提供 source roots 后，Asset Cooker 默认应处理哪些资产？ → A: 默认处理显式 root Asset 及其完整传递依赖，同时提供必须显式选择的 cook-all 模式来处理 source roots 下所有可发现资产。
- Q: 当两个 Asset Cooker 进程同时写入同一个 target publication root 时，后启动的进程应如何处理？ → A: 使用跨进程排他 publication lease；后启动者有界等待，超时后明确失败。
- Q: 普通 cook 命中损坏或不兼容的 derived cache entry 时，默认行为应是什么？ → A: 隔离损坏条目并记录确定性诊断，普通 cook 按 cache miss 重建；显式 strict cache validation 必须失败。
- Q: 两个名称不同但有效配置相同的 target profile 是否应产生相同 cooked outputs 与 derived keys？ → A: 是；规范化有效配置决定 profile 的派生身份，名称仅用于显示。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Produce Deterministic Cooked Assets (Priority: P1)

An engine developer can select a repository content root and an explicit target
profile, run an offline cook, and receive validated cooked payloads plus a
manifest that preserves the same typed asset identities and logical content as
the development source-import path.

**Why this priority**: A deterministic cooked output is the minimum useful
delivery unit for every later runtime manager and native backend. Without it,
runtime loading remains coupled to source formats and developer-only importers.

**Independent Test**: Cook a representative corpus containing Feature 021
images/textures, Feature 022 KTX2 outputs, Feature 023 materials/shaders, and
Feature 024 static models/meshes twice from clean output roots; validate both
runs and compare normalized manifests, payload digests, dependency records,
identities, and inspection reports.

**Acceptance Scenarios**:

1. **Given** valid source assets and a supported target profile, **When** a
   clean cook completes, **Then** every reachable asset has exactly one
   manifest record and a validated cooked payload appropriate to that target.
2. **Given** identical source bytes, dependency bytes, target-profile
   `BuildPolicy` including versioned producer settings, and remaining target evidence,
   **When** the corpus is cooked repeatedly or on another
   supported host, **Then** normalized manifests, derived keys, payload digests,
   dependency order, and diagnostics are identical.
3. **Given** a source asset that emits several typed subresources, **When** it
   is cooked, **Then** every subresource retains its Feature 020 `FAssetId` and
   appears independently in the manifest without identity being derived from
   an output path or content hash.
4. **Given** semantically equivalent development-import and cooked-load paths,
   **When** their typed payloads are inspected, **Then** they agree on asset
   identity, schema version, dependency meaning, and normalized content.
5. **Given** a target profile unsupported by one required asset, **When** cook
   is requested, **Then** the operation fails before publishing a complete
   manifest and identifies the asset, target capability, and reason.
6. **Given** explicit root assets, **When** the default cook mode runs, **Then**
   only those roots and their complete required dependency closure are included.
7. **Given** the explicit cook-all mode, **When** discovery completes, **Then**
   every supported asset discoverable under the declared source roots and its
   required dependency closure are included in deterministic order.

---

### User Story 2 - Reuse Valid Derived Data Incrementally (Priority: P1)

An engine developer can repeat a cook after changing part of the source graph
and have unchanged outputs reused while the changed assets and all transitively
affected dependents are rebuilt from complete version evidence.

**Why this priority**: Full recooking does not scale with project growth.
Correct incremental invalidation is also the proof that derived data is a cache
rather than a second source of truth.

**Independent Test**: Populate a derived-data store, repeat the cook unchanged,
then separately change source bytes, importer/cooker revision, target-profile
build policy or capabilities, direct dependencies, and transitive dependencies;
compare hit/miss,
invalidation, rebuild, and output records against an independently computed
expected graph.

**Acceptance Scenarios**:

1. **Given** a completed cook and unchanged complete inputs, **When** the same
   target is cooked again, **Then** all eligible payloads are cache hits and no
   payload is regenerated.
2. **Given** one leaf source changes, **When** cook is repeated, **Then** that
   asset and every dependent whose output evidence includes it are rebuilt,
   while unrelated graph branches remain reusable.
3. **Given** only an importer, cooker, schema, target-profile build-policy
   setting, or target capability changes, **When** cook is repeated, **Then**
   precisely the outputs governed by that evidence receive new derived keys and
   are rebuilt.
4. **Given** the same `FAssetId` now resolves to a new content version, **When**
   cook is repeated, **Then** the manifest preserves identity while recording
   the new source, dependency, derived-key, and payload evidence.
5. **Given** a corrupt, incomplete, or mismatched cache entry, **When** an
   ordinary cook queries it, **Then** the entry is quarantined with deterministic
   evidence, treated as a miss, and rebuilt without ever being published as
   valid output.
6. **Given** a corrupt, incomplete, or mismatched cache entry, **When** explicit
   strict cache validation runs, **Then** validation fails and identifies the
   derived key, violated invariant, and quarantine disposition.
7. **Given** unreachable stale entries in the cache, **When** a cook completes,
   **Then** they do not appear in the new manifest and do not alter its digest.

---

### User Story 3 - Publish and Validate Outputs Atomically (Priority: P1)

An engine developer or build system can publish a target output without
readers observing a mixture of old and new payloads, and can validate an
existing output tree without access to source importers.

**Why this priority**: A manifest that references missing, partial, stale, or
wrong-target payloads is not a deployable artifact. Atomic publication is the
boundary between temporary build work and a runtime-consumable generation.

**Independent Test**: Inject failure at every pre-commit staging, payload,
manifest, validation, and publication boundary; inject a post-commit audit
warning and simulate interruption at the atomic replacement boundary while
concurrently inspecting the current generation; then corrupt, remove,
substitute, and add payloads before running standalone validation.

**Acceptance Scenarios**:

1. **Given** a valid previously published generation, **When** a new cook fails
   before the atomic `Current.json` replacement commit point, **Then** the
   previous generation remains complete, readable, and current, and the failed
   generation is not advertised as current.
2. **Given** a successful staged cook, **When** publication occurs, **Then** one
   complete validated generation becomes current as a single observable state
   change. Once atomic replacement reports success, the cook is committed and
   returns success; a later audit read failure is diagnostic and MUST NOT
   reclassify the committed publication as a failed cook.
3. **Given** an existing cooked output with all referenced payloads, **When**
   standalone validation runs, **Then** it verifies manifest structure, target
   compatibility, asset identities, versions, dependencies, payload sizes,
   digests, and schemas without source files.
4. **Given** a missing, truncated, corrupt, stale, path-escaping, duplicate, or
   target-incompatible payload, **When** validation runs, **Then** validation
   fails closed and identifies the manifest record and violated invariant.
5. **Given** files not referenced by the current manifest, **When** validation
   runs in strict mode, **Then** unexpected files are reported without being
   mistaken for runtime assets.
6. **Given** two cook requests targeting the same publication root, **When**
   they overlap, **Then** one holds an exclusive cross-process publication lease
   while the other waits for a bounded interval and fails explicitly on timeout;
   their manifest and payload generations cannot interleave.
7. **Given** any snapshotted source or dependency changes after planning,
   **When** processing or pre-publication verification detects the new version,
   **Then** the cook fails without publishing and the current generation remains
   unchanged.

---

### User Story 4 - Define Explicit Target Profiles (Priority: P2)

An engine developer can describe a versioned target profile whose platform,
graphics payload families, texture capabilities, build policy, and validation
limits determine cooked outputs without relying on the host machine's implicit
capabilities.

**Why this priority**: Cooked data must represent the deployment target rather
than whichever workstation performed the build. Explicit profiles also provide
the contract needed by Metal, DirectX, OpenGL, meshlet, and later runtime phases.

**Independent Test**: Cook the same source graph for several fixture profiles
that vary shader payload family, compressed texture capability, fallback
policy, and limits; verify deterministic selection, distinct derived keys only
where outputs differ, and rejection of incomplete or contradictory profiles.

**Acceptance Scenarios**:

1. **Given** a complete supported profile, **When** assets are cooked, **Then**
   every target-sensitive choice is attributable to a versioned profile field
   recorded in the manifest and derived evidence.
2. **Given** two profile names with identical normalized effective
   configuration, **When** the same assets are cooked, **Then** target-sensitive
   decisions, derived keys, payload digests, and generation identity are
   identical apart from non-authoritative display metadata.
3. **Given** the same target profile on Windows, macOS, or Linux hosts, **When**
   source tools support that profile, **Then** host-only paths, clocks, locale,
   environment variables, and filesystem ordering do not alter outputs.
4. **Given** profiles that differ only in a field irrelevant to one asset type,
   **When** both are cooked, **Then** that asset may reuse identical derived
   data while target-sensitive assets receive distinct keys.
5. **Given** a profile requiring unavailable shader or texture payloads,
   **When** cook runs, **Then** it follows only an explicitly declared fallback
   or fails; it never silently substitutes a host-preferred format.
6. **Given** an unknown profile revision or field that changes interpretation,
   **When** it is consumed, **Then** cook and validation fail with an actionable
   compatibility diagnostic.

---

### User Story 5 - Inspect, Diagnose, and Reproduce a Cook (Priority: P3)

An engine maintainer can inspect a cook plan and result, explain why an asset
was rebuilt or reused, reproduce the operation on a clean machine, and
distinguish source, dependency, profile, cache, payload, and publication
failures without parsing host-specific logs.

**Why this priority**: Deterministic data is only maintainable when its evidence
is understandable. Diagnostics turn cache behavior and invalidation from hidden
state into a testable engineering contract.

**Independent Test**: Exercise clean, dry-run, unchanged, partially changed,
corrupt-cache, missing-dependency, unsupported-target, interrupted-publication,
and validation-only cases; compare normalized reports and result categories
across supported hosts.

**Acceptance Scenarios**:

1. **Given** a cook request, **When** planning is requested without publication,
   **Then** the report lists roots, reachable assets, deterministic order,
   expected hits/misses, invalidation reasons, and intended outputs without
   changing the cache or publication root.
2. **Given** a cache hit or miss, **When** the result is inspected, **Then** the
   report identifies the complete evidence responsible for reuse or rebuild.
3. **Given** a failed asset, **When** diagnostics are inspected, **Then** they
   identify stable asset identity, dependency chain, processing stage, target,
   normalized category, and actionable reason without leaking temporary paths.
4. **Given** a clean checkout plus declared tool dependencies and source
   content, **When** the documented cook command is run, **Then** it can recreate
   and validate the same normalized target generation.
5. **Given** the same normalized failure on different hosts, **When** reports
   are compared, **Then** categories and stable subjects match apart from
   explicitly identified host evidence.

### Edge Cases

- The selected content root is empty, duplicated, absent, outside the allowed
  source scope, or resolves through aliases to the same logical root.
- Two discovered source files or emitted subresources claim the same typed
  `FAssetId` with different content or type evidence.
- A dependency is missing, cyclic, self-referential, resolved outside the
  allowed scope, changes during cook, or has a type incompatible with its role.
- Filesystem enumeration differs by host, is case-insensitive on one host, or
  contains names that normalize to the same logical path.
- Source or output paths contain spaces, non-ASCII text, reserved names, long
  components, symbolic links, junctions, or `.`/`..` traversal.
- A source, dependency, intermediate result, manifest, or payload exceeds a
  configured count or byte limit; size arithmetic overflows.
- The system clock changes, locales differ, the checkout root differs, or a
  host exposes different temporary-directory names and environment variables.
- An asset has zero dependencies, thousands of dependencies, a diamond graph,
  or a deep but valid acyclic dependency chain.
- A content hash collides in a synthetic test, a digest string is malformed,
  or stored size/digest evidence disagrees with payload bytes.
- An importer emits outputs in nondeterministic order or changes outputs without
  changing its declared revision.
- A cooker crashes or loses power while writing staging data, cache data, the
  manifest, validation evidence, or the publication marker.
- A previous generation is read while a replacement is staging or publishing.
- Two processes request the same derived key concurrently, or target the same
  publication root with identical or different input generations.
- A cache entry has valid naming but missing metadata, wrong target evidence,
  incompatible schema, truncated bytes, extra bytes, or a digest mismatch.
- A manifest references the same payload under several assets, several payloads
  for one asset, an unreachable asset, or a dependency absent from the manifest.
- A target profile is incomplete, contradictory, unknown, deprecated, or asks
  for a texture/shader payload family unavailable from source dependencies.
- A clean cook succeeds but standalone validation later observes an edited,
  missing, permission-denied, or replaced payload.
- Cleanup cannot remove failed staging data or stale cache entries; this must
  not invalidate the currently published generation.
- An incremental cook removes or renames an asset that remains referenced by a
  dependent, or leaves an old derived entry reachable only by stale metadata.
- Development import and cooked decoding disagree on schema revision, numeric
  normalization, dependency order, texture semantics, material bindings, mesh
  coordinates, or subresource identity.

## Architecture & Design Constraints *(mandatory)*

- **Layer Boundary**: Offline cooker tooling MAY depend on Asset and Core.
  Runtime Asset, RHI, Backend, Renderer, and Application modules MUST NOT depend
  on Tools or invoke the cooker.
- **Asset Ownership**: Asset remains responsible for CPU payload contracts,
  identities, versions, dependencies, import/cook/load contracts, and cooked
  validation. It MUST NOT own GPU objects or depend on RHI, Renderer,
  Application, Backend, or a graphics API.
- **Identity Separation**: `FAssetId` is a stable typed logical identity.
  Source, content, dependency, profile `BuildPolicy`/producer settings, profile,
  cooker, and payload hashes are version evidence and MUST NOT become the
  logical identity.
- **Cache Authority**: Derived data is disposable and reconstructible. Source
  content plus declared processing inputs remain authoritative; cache contents
  MUST never silently override them.
- **Publication Boundary**: A cooked target generation MUST be validated before
  it becomes current, and readers MUST observe either the prior complete
  generation or the new complete generation.
- **Determinism**: Host paths, clocks, locale, process identifiers, temporary
  names, environment ordering, and filesystem enumeration MUST NOT influence
  normalized cooked outputs.
- **Design Patterns**: Discovery, graph planning, processing, storage,
  validation, and publication are orthogonal responsibilities and MUST NOT be
  concentrated into one god-class or giant function.
- **Advanced Graphics**: Manifests, derived keys, and extension contracts MUST
  allow later meshlet, ray-tracing, SDF, shader-backend, and streaming-derived
  payloads without making those payloads part of Feature 025.
- **Naming Conventions**: New engine-facing code contracts MUST follow
  PascalCase and Unreal Engine-style project naming conventions. User-facing
  command terms and serialized field names require one documented canonical
  spelling.
- **Cross-Platform Compatibility**: Cook planning, output, validation, and
  normalized diagnostics MUST be supported on Windows, macOS, and Linux.
  Platform-specific filesystem behavior MUST be isolated and normalized.
- **Automated Cross-Platform Validation**: The feature MUST include automated
  Windows, macOS, and Linux clean and incremental cook validation, plus strict
  release and applicable sanitizer coverage. Any temporary gap requires a
  documented manual fallback and follow-up task.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST provide an offline Asset Cooker entry point that
  accepts explicit source roots, asset-selection mode, output root,
  derived-data root, and a target profile whose `BuildPolicy` contains every
  caller-controlled payload-affecting processing setting, without requiring a
  running Application or Renderer. Feature 025 has no separate implicit or
  host-local processing-settings channel. After canonical containment checks,
  source scopes, output root, and derived-data root MUST be pairwise
  non-overlapping; an explicit report path MUST NOT alias any source, cache,
  manifest, payload, or current-pointer path.
- **FR-002**: A cook request MUST identify one versioned target profile and MUST
  fail before processing when the profile is missing, malformed, contradictory,
  or unsupported.
- **FR-003**: The cooker MUST default to processing explicitly selected root
  Assets plus their complete required transitive dependency closure, and MUST
  also provide an explicit cook-all mode that selects every supported asset
  discoverable under the declared source roots plus their dependency closure.
  Discovery MUST use the Feature 020 resolver/importer contracts and concrete
  Features 021-024 payload contracts without hard-coding one checkout path. An
  explicit-root request with no roots or a cook-all request that discovers no
  supported typed outputs MUST fail as `DiscoveryFailure` before processing or
  publication; Feature 025 does not publish empty generations.
- **FR-004**: Discovery MUST normalize source locators and produce a stable
  order independent of filesystem enumeration, host case rules, and locale.
  Source-scope aliases that resolve to the same canonical scope MUST collapse
  to one scope; absent, out-of-scope, or mutually overlapping source scopes
  MUST fail before discovery mutates cache or publication state.
- **FR-005**: Duplicate or conflicting typed identities discovered from source
  files or emitted subresources MUST fail the affected cook before publication.
- **FR-006**: The cooker MUST construct a complete directed dependency graph and
  record the version of every selected source and required transitive dependency
  before processing; it MUST verify those versions during processing and again
  before publication, and any change MUST fail the cook without publication.
- **FR-007**: Dependency planning MUST reject missing, type-incompatible,
  self-referential, cyclic, out-of-scope, or limit-exceeding relationships with
  a stable identity path explaining the failure.
- **FR-008**: Processing order MUST be deterministic and dependency-safe; ties
  MUST be resolved by stable typed asset identity rather than internal storage
  or task completion order.
- **FR-009**: One source import that emits multiple typed subresources MUST
  preserve each output as a separate graph node and manifest record.
- **FR-010**: Every cooked output MUST preserve the same `FAssetId`, asset type,
  schema meaning, and dependency roles as the equivalent development-import
  output.
- **FR-011**: Development-import and cooked-load representations MAY differ in
  physical encoding but MUST have a defined normalized semantic-equivalence
  comparison for each Feature 021-024 payload type.
- **FR-012**: The initial cooker MUST support all in-scope payloads delivered by
  Features 021, 022, 023, and 024, including image/texture, KTX2, material,
  shader, material-instance, static-mesh, and static-model assets.
- **FR-013**: Runtime asynchronous requests, runtime handles, in-process runtime
  cache ownership, streaming, hot reload, network storage, and GPU residency
  MUST NOT be introduced by this feature.
- **FR-014**: Each target profile MUST have a schema revision, human-readable
  display name, and normalized effective configuration covering at least target
  platform, CPU architecture, graphics backend, shader payload family, texture
  capabilities and fallback policy, and every build policy capable of changing
  payloads. `BuildPolicy` MUST contain a unique, sorted, schema-versioned settings
  record for every selected producer; each producer MUST reject missing,
  duplicate, unknown, or invalid settings. The display name MUST NOT be
  authoritative identity evidence.
- **FR-015**: Target-sensitive shader and texture choices MUST come from the
  explicit profile and source evidence, never from undeclared host capability
  probing.
- **FR-016**: An unsupported target-sensitive requirement MUST either follow a
  fallback explicitly authorized by the profile or fail closed with no silent
  substitution.
- **FR-017**: Changing a profile field MUST invalidate only outputs whose
  processing or validation evidence depends on that field. Renaming a profile
  without changing its normalized effective configuration MUST NOT invalidate
  outputs or change derived keys.
- **FR-018**: Every derived output MUST have a deterministic derived key that
  covers at least source content/version evidence, all relevant dependency
  versions, importer revision, cooker revision, payload schema revision,
  the cooker projection of its schema-versioned target-profile `BuildPolicy`
  producer-settings record and all other relevant target-profile evidence.
- **FR-019**: Derived keys MUST use canonical type-tagged and length-delimited
  evidence so that different input boundaries cannot produce an ambiguous key.
- **FR-020**: Derived-key computation MUST exclude unstable host evidence such
  as absolute checkout paths, timestamps, process identifiers, locale, and
  filesystem traversal order.
- **FR-021**: The complete evidence used to compute a derived key MUST be
  inspectable in normalized form without exposing raw temporary paths.
- **FR-022**: Derived data MUST be content-validated before reuse, including key,
  payload type, schema revision, byte size, digest, and target compatibility.
- **FR-023**: An ordinary cook MUST quarantine a corrupt, incomplete,
  incompatible, or unrecognized derived entry with deterministic evidence and
  treat it as a cache miss before rebuilding. Explicit strict cache validation
  MUST fail on that entry. A missing entry is a miss; no invalid entry may be
  accepted or deleted without diagnostic evidence.
- **FR-024**: Concurrent requests for the same derived key MUST not expose a
  partial entry or produce conflicting valid entries.
- **FR-025**: Derived storage MUST be local and reconstructible for this phase;
  no network service, remote cache protocol, or persistent editor database is
  in scope.
- **FR-026**: An unchanged incremental cook MUST reuse all eligible validated
  derived outputs and report why each output was reused. For Feature 025, the
  eligibility denominator is every reachable, successfully planned node whose
  complete derived key can be computed; all registered Feature 025 cookers are
  deterministic and cacheable. Unsupported or failed planning nodes are
  excluded with an explicit reason, and an explicit clean cook bypasses DDC
  lookup and is not an incremental-reuse measurement.
- **FR-027**: A changed source, dependency, importer/cooker revision, schema,
  relevant schema-versioned `BuildPolicy` producer setting, or relevant profile
  capability MUST invalidate the affected output and every dependent whose
  derived evidence changes.
- **FR-028**: Unrelated graph branches MUST remain reusable after a localized
  source or settings change.
- **FR-029**: Removed or unreachable assets MUST not remain in the newly
  published manifest even if their cache entries remain stored.
- **FR-030**: The cooker MUST support a plan-only mode that reports expected
  graph order, cache decisions, invalidation causes, and publication changes
  without modifying derived storage or published output.
- **FR-031**: A manifest MUST identify its schema revision, target profile
  display name, normalized effective-profile evidence, generation identity,
  selection mode, normalized root assets, and complete deterministic asset
  record order; cook-all root evidence MUST identify the normalized source
  scopes from which the selected roots were discovered.
- **FR-032**: Each manifest asset record MUST include typed `FAssetId`, asset
  type, payload schema revision, source/version evidence, relevant derived key,
  payload locator, byte size, digest, and typed dependency records.
- **FR-033**: Manifest payload locators MUST be relative to the published root,
  canonical, traversal-free, and portable across supported host path syntaxes.
- **FR-034**: Manifest serialization MUST have one canonical ordering and
  encoding so equivalent generations produce byte-identical normalized
  manifests.
- **FR-035**: Manifest generation identity MUST derive from normalized content
  and target evidence rather than wall-clock time or a random identifier.
- **FR-036**: The cooker MUST write new outputs to an isolated staging
  generation and MUST NOT modify the current published generation in place.
- **FR-037**: Before publication, the staged generation MUST pass the same
  standalone validation contract available to later consumers.
- **FR-038**: Publication MUST make one complete validated generation current
  atomically from a reader's perspective. Every reported failure before the
  atomic `Current.json` replacement commit point MUST preserve the previous
  current generation. Once replacement reports success, publication is
  committed and MUST return success; post-commit audit diagnostics cannot claim
  that the previous generation remains current.
- **FR-039**: A writer MUST hold an exclusive cross-process publication lease
  for one publication root before mutating its staging or current-generation
  state. A competing writer MUST wait for a configurable bounded interval and
  fail with a stable timeout result if the lease remains unavailable; payload
  and manifest generations MUST never interleave.
- **FR-040**: Failed staging and cleanup artifacts MUST not be discoverable as
  current runtime content, even when cleanup itself cannot complete.
- **FR-041**: Standalone validation MUST operate using only the published
  manifest, target profile evidence, and referenced payloads; it MUST NOT
  require source files or development importers.
- **FR-042**: Standalone validation MUST verify manifest schema, target
  compatibility, unique identities, dependency closure and acyclicity, payload
  locator safety, byte sizes, digests, payload schemas, and generation evidence.
- **FR-043**: Strict validation MUST report unexpected files separately from
  referenced asset payloads and MUST reject ambiguous or duplicate locators.
- **FR-044**: Payload and manifest parsers MUST enforce explicit limits for
  bytes, records, dependencies, nesting, path length, and diagnostic volume,
  with checked arithmetic at every size-derived boundary.
- **FR-045**: A failure before the publication commit point MUST leave
  caller-visible result objects, current publication, and unrelated validated
  cache entries unchanged. A successfully committed publication returns success
  with complete immutable result evidence even if an optional post-commit audit
  read produces a diagnostic.
- **FR-046**: Diagnostics MUST identify stable asset identity when available,
  dependency chain, target profile, processing stage, normalized category, and
  actionable reason without exposing third-party-library-only messages as the
  public contract.
- **FR-047**: Inspection MUST explain every cache hit, miss, invalidation,
  rebuild, fallback, validation decision, and publication outcome using stable
  normalized evidence.
- **FR-048**: Summary reports MUST include discovered, reachable,
  reuse-eligible, reuse-ineligible, cooked, regenerated, reused, invalidated,
  failed, and published counts plus total input/output bytes. Every ineligible
  node MUST have a stable exclusion reason, while deterministic artifacts MUST
  exclude elapsed time and other unstable measurements.
- **FR-049**: Repeated clean cooks of identical complete input on supported
  hosts MUST produce identical normalized manifests, derived keys, payload
  digests, dependency order, and result categories.
- **FR-050**: Repeated incremental cooks MUST converge to the same normalized
  generation as a clean cook of the final source state.
- **FR-051**: Fixture metadata MUST record provenance, license, expected
  validity, represented asset types, target profiles, and expected normalized
  outcomes for all checked-in cooker corpus entries.
- **FR-052**: Validation MUST cover clean-machine recreation, clean cook,
  unchanged incremental cook, partial invalidation, removed assets, corrupt and
  stale cache data, interrupted publication, strict standalone validation, and
  concurrent same-key/same-root requests.
- **FR-053**: Automated validation MUST build and run focused Feature 025 tests
  on Windows, macOS, and Linux, with strict Release and applicable sanitizer
  coverage, and retain normalized reports as inspectable evidence.
- **FR-054**: Architecture validation MUST prove that runtime modules do not
  depend on Tools, Asset does not depend on graphics/runtime layers, and cooker
  code does not bypass public Asset/Core contracts into private import details.
- **FR-055**: The cooker and validation entry points MUST provide stable process
  result categories suitable for local scripts and hosted automation.
- **FR-056**: The feature MUST document one clean-machine workflow that creates,
  validates, inspects, and reproduces a target generation from declared project
  inputs.
- **FR-057**: Extension contracts MUST permit future derived payload producers
  such as meshlet clusters, ray-tracing data, SDF data, and backend shader
  payloads without requiring those producers in Feature 025.
- **FR-058**: The output contract MUST be consumable by the later Runtime Asset
  Manager without embedding runtime scheduling, handle lifetime, eviction, or
  GPU residency policy into the manifest or cooker.

### Key Entities

- **Cook Request**: Immutable description of source roots, selected root assets,
  target profile including payload-affecting `BuildPolicy`, output and
  derived-data locations, limits, validation strictness, and
  clean/incremental/plan-only policy.
- **Target Profile**: Versioned deployment contract whose normalized effective
  configuration, rather than display name, determines target-sensitive derived
  identity. It describes platform, CPU architecture, graphics backend, shader
  and texture payload capabilities, fallback policy, payload-affecting build
  policy, and limits independently of the host performing the cook.
- **Cook Graph**: Deterministic directed acyclic graph of typed assets,
  subresources, dependencies, processing stages, and version evidence reachable
  from selected roots.
- **Cook Node**: One typed Asset identity plus its source/import evidence,
  dependencies, effective schema-versioned `BuildPolicy` producer-settings projection,
  target-relevant profile evidence, derived key, processing state, and output
  result.
- **Derived Key**: Deterministic version key over all inputs capable of changing
  one cooked payload. It locates reusable evidence but is not an Asset identity.
- **Derived Entry**: Reconstructible locally stored payload plus metadata needed
  to prove key, type, schema, size, digest, and target compatibility before use.
- **Quarantined Derived Entry**: Invalid cache entry removed from eligibility for
  reuse while preserving stable key, failure category, violated evidence, and
  disposition information for diagnosis or later cleanup.
- **Cooked Payload**: Validated target-ready CPU byte representation of one typed
  asset, decodable without its authoring source file.
- **Manifest**: Canonical target-generation index mapping typed Asset identities
  to payloads, versions, dependencies, schemas, and validation evidence.
- **Published Generation**: One complete immutable manifest and its referenced
  payload set that has passed standalone validation and is atomically visible as
  the current output.
- **Publication Lease**: Exclusive cross-process ownership record for one target
  publication root, with bounded acquisition waiting, stale-owner detection,
  release, and timeout evidence; it protects generation publication rather than
  locking source authoring files.
- **Cook Report**: Normalized explanation of graph planning, cache decisions,
  invalidation, processing, validation, publication, counts, and failures.
- **Cooker Extension**: Registered producer for one derived payload family that
  declares accepted input types, revision evidence, target dependencies, output
  schema, validation, and deterministic processing contract.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Two clean cooks of the complete representative corpus on each
  supported host produce byte-identical normalized manifests and identical
  derived keys, payload digests, dependency order, and result categories.
- **SC-002**: A clean cook and a fully cached incremental cook of identical
  inputs produce the same generation identity; the incremental run reports
  100% eligible payload reuse and zero regenerated payloads.
- **SC-003**: For a mutation matrix covering source bytes, direct dependency,
  transitive dependency, importer revision, cooker revision, schema revision,
  target-profile producer-settings schema/value, and relevant profile
  capability, 100% of expected nodes are invalidated and 100% of
  unrelated nodes remain reusable.
- **SC-004**: After any sequence of incremental additions, edits, removals, and
  renames in the validation corpus, the final incremental generation matches a
  clean cook of the same final source state in every normalized artifact.
- **SC-005**: Failure injection at every pre-commit staging, payload, manifest,
  validation, and publication boundary leaves the previously published
  generation current and valid in 100% of tested cases. Injection immediately
  after a successful atomic replacement reports committed success with a stable
  audit diagnostic. Simulated process interruption at the commit boundary may
  expose either the complete old or complete new pointer, never partial JSON or
  a mixed generation.
- **SC-006**: Standalone validation detects 100% of a corpus containing at least
  30 targeted manifest, path, dependency, schema, size, digest, target, missing,
  truncation, substitution, and unexpected-file corruptions.
- **SC-007**: The representative corpus includes at least one successfully
  cooked and validated payload of every in-scope Feature 021-024 asset type and
  at least one multi-subresource model package.
- **SC-008**: Development-import and cooked-load normalized comparison passes
  for 100% of valid representative payloads across identity, schema, dependency
  roles, and semantic content.
- **SC-009**: At least eight concurrent same-key requests and two overlapping
  same-root publication requests complete or reject deterministically with no
  partial entries, mixed generations, or differing valid payload digests.
- **SC-010**: A representative graph containing at least 1,000 assets and 5,000
  dependency edges completes planning, clean cooking, unchanged incremental
  reuse, and standalone validation within documented resource budgets agreed
  during planning.
- **SC-011**: Twenty repeated runs of plan-only, clean cook, incremental cook,
  and validation produce identical normalized reports for the same inputs.
- **SC-012**: A clean checkout on each supported host can follow the documented
  workflow to cook and validate the representative target without undeclared
  machine-local state.
- **SC-013**: Architecture checks report zero runtime-to-Tools dependencies,
  zero Asset-to-RHI/Renderer/Application/Backend dependencies, and zero private
  importer-boundary violations.
- **SC-014**: Windows, macOS, and Linux focused validation, strict Release
  validation, applicable sanitizer suites, and all pre-existing regression
  suites pass on the final feature revision.
- **SC-015**: At least two differently named profiles with identical effective
  configuration produce identical derived keys, payload digests, and generation
  identity, while mutations of every payload-affecting profile dimension
  invalidate all and only the dependent outputs.

## Assumptions

- Feature 020 stable typed identity, version, resolver, importer, registry, and
  diagnostic contracts remain authoritative.
- Features 021-024 provide the first complete payload families and enough
  normalized inspection to compare source-imported and cooked representations.
- Feature 022 KTX2 bytes are reused as the initial cooked texture payload rather
  than inventing a second texture container.
- Feature 023 shader payload selection is driven by explicit target-profile
  evidence; additional backend payload production can register later without
  changing existing Asset identities.
- The initial derived-data store and publication roots are local filesystem
  locations. Shared network caches, content delivery, and editor databases are
  future work.
- The initial manifest describes one target generation and uses relative
  payload locators. Archive/package aggregation and patch distribution are not
  required in this phase.
- The current published generation is immutable. A new cook publishes a new
  generation rather than editing payloads in place.
- Concurrent same-root writers use an exclusive cross-process publication lease
  with configurable bounded waiting and explicit timeout failure; distributed
  locking across machines is outside the local-only scope.
- Wall-clock timing may be collected as non-deterministic telemetry but is not
  included in manifests, derived keys, generation identities, or normalized
  golden reports.
- Cooked payload semantic equivalence does not require identical physical bytes
  to development in-memory payloads where target compression or serialization
  differs.
- Runtime source fallback, asynchronous request scheduling, typed runtime
  handles, cache lifetime, cancellation, and unloading are Feature 026 scope.
- Meshlet, ray-tracing, SDF/surface-cache, backend-native shader, streaming, and
  residency payloads may use Feature 025 extension contracts later but are not
  delivered here.
