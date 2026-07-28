# Feature Specification: Asset Core, Identity & Registry

**Feature Branch**: `020-asset-core`
**Created**: 2026-07-28
**Status**: Approved
**Input**: User description: "基于 Roadmap 2.1 的下一阶段，建立 Asset Core、Identity 与 Registry 基础"

## Clarifications

### Session 2026-07-28

- Q: 当注册 Asset Metadata 时，其 required dependency 指向的目标 Asset 尚未注册，FAssetRegistry 应该如何处理本次注册？ → A: 接受注册并将依赖显式标记为 `Unresolved`；目标注册后自动变为 resolved，任何依赖完整性验证或后续消费前都必须确认 required dependencies 已 resolved。
- Q: 当多个 IAssetResolver 同时符合一个 logical source request，并且拥有相同的最高优先级时，registry 应该如何决定最终 resolver？ → A: 返回 `AmbiguousResolver` 并列出稳定排序的冲突候选；participant identity 只用于稳定诊断，不作为隐藏的功能性 tie-breaker。
- Q: FAssetRegistry 是否需要在 Feature 020 就支持多线程访问；当查询与注册、替换或移除同时发生时，应保证什么行为？ → A: 允许多线程并发查询，写操作由 registry 内部串行化；每个原子 batch 对读者只呈现完整的修改前状态或修改后状态。
- Q: 当 logical path 或 subresource 包含视觉相同但底层 Unicode 编码序列不同的文本时，它们应当被视为同一个 Asset Identity 吗？ → A: logical path 与 subresource 在形成 identity 前统一规范化为 Unicode NFC；规范化后相同即为同一 identity，同时保持大小写敏感。
- Q: 当 resolver、importer、loader 或 cooker 的 registration token 被注销时，如果该 extension 已被请求选中并正在执行，系统应如何处理？ → A: 注销立即阻止新请求选择该 extension；已开始的请求持有 execution lease 并可安全完成，extension 实例在最后一个 lease 释放后才可销毁。

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Refer to Assets by Stable Logical Identity (Priority: P1)

An engine developer can identify an asset by its type, canonical logical path,
and optional subresource without embedding a machine-specific filesystem path
or a particular content version in long-lived references.

**Why this priority**: Stable identity is the foundation for every later image,
texture, material, model, cooking, and runtime-management feature.

**Independent Test**: Create identities and typed soft references from valid and
invalid textual inputs, round-trip their canonical form, and compare the results
across repeated runs and supported operating systems.

**Acceptance Scenarios**:

1. **Given** equivalent logical paths using accepted separator variations,
   **When** identities are created, **Then** they resolve to one canonical,
   platform-independent identity.
2. **Given** the same logical path with two different asset types or
   subresources, **When** identities are compared, **Then** they remain distinct
   and retain their type information.
3. **Given** an identity and multiple content or cook revisions, **When** a
   revision changes, **Then** the stable identity remains unchanged while its
   version information changes.
4. **Given** a typed soft reference, **When** its identity has a mismatched asset
   type, **Then** the mismatch is rejected before a payload is used.

---

### User Story 2 - Register and Inspect Asset Metadata (Priority: P1)

An engine developer can register metadata for individual assets or all
subresources emitted by one source, query that metadata deterministically, and
inspect direct and reverse dependencies without loading asset content.

**Why this priority**: Later importers, cookers, and runtime loading need one
authoritative in-memory view of asset identity, provenance, version, and
dependency relationships.

**Independent Test**: Register a representative multi-asset source graph,
perform identity, type, source, and dependency queries, update or remove
records, and compare the normalized registry dump over repeated runs.

**Acceptance Scenarios**:

1. **Given** valid metadata for one source and its typed subresources, **When**
   the records are registered as one batch, **Then** all records become visible
   together in deterministic identity order.
2. **Given** an identity already registered with conflicting metadata, **When**
   another registration is attempted, **Then** the conflict is rejected and
   the prior registry state remains unchanged.
3. **Given** an asset with required dependencies, **When** a target is not yet
   registered, **Then** the batch is accepted with an inspectable `Unresolved`
   edge; a known required-dependency cycle still rejects the entire batch.
4. **Given** a registered dependency graph, **When** an asset is queried or
   removed, **Then** direct dependencies, reverse dependents, resolved-to-
   unresolved transitions, and resulting incomplete references are reported
   consistently.
5. **Given** concurrent readers and a metadata batch mutation, **When** the
   batch is published, **Then** every reader observes either the complete
   pre-batch state or the complete post-batch state and never a partial index.

---

### User Story 3 - Extend Asset Discovery and Transformation (Priority: P2)

An engine developer can register storage resolvers and asset processing
extensions, then deterministically select the one valid participant for a
request without coupling the Asset layer to a concrete file format or storage
provider.

**Why this priority**: Feature 020 must establish extension contracts that
Features 021-026 can use without revising core identity or registry behavior.

**Independent Test**: Use test resolvers, importers, loaders, and cookers with
overlapping claims, probe outcomes, multiple subresource outputs, failures, and
lifecycle changes; verify dispatch and diagnostics without a real image, model,
database, or graphics runtime.

**Acceptance Scenarios**:

1. **Given** a logical source request and multiple registered resolvers, **When**
   resolution is requested, **Then** the unique highest-priority eligible
   resolver is selected; equal highest-priority candidates produce
   `AmbiguousResolver` rather than an order-dependent choice.
2. **Given** candidate importers selected by declared format support, **When**
   bounded content probing is performed, **Then** the strongest unique valid
   match is selected and tied matches fail rather than depending on registration
   order.
3. **Given** one source that emits several typed assets, **When** import
   discovery succeeds, **Then** every output has a stable subresource identity,
   metadata, version information, and declared dependencies.
4. **Given** an extension that fails, is unregistered, or outlives its valid
   owner, **When** unregistration races with dispatch, **Then** no new request
   selects it, already-started requests complete through valid execution leases,
   no stale callback is invoked, and failures identify the stage and
   participant.

---

### User Story 4 - Diagnose and Evolve Asset Foundations Safely (Priority: P3)

An engine maintainer can inspect identity, registry, dispatch, and lifecycle
decisions; run focused Asset validation; and confirm that the new layer remains
portable and independent from rendering systems.

**Why this priority**: The Asset layer will become a broad dependency. Its first
contracts must be observable, test-selectable, and protected from architectural
drift before concrete formats are added.

**Independent Test**: Run the focused Asset test suite and architecture checks,
repeat deterministic reports, then run full regression and supported-platform
automation without graphics hardware or source asset codecs.

**Acceptance Scenarios**:

1. **Given** equivalent operations, **When** diagnostics and inspection dumps
   are generated repeatedly, **Then** ordering, result categories, and text are
   stable and contain no native addresses.
2. **Given** the Asset production layer, **When** dependency boundaries are
   checked, **Then** it depends only on Core and has no RHI, Renderer,
   Application, Backend, Tools, or graphics API dependency.
3. **Given** the project test executable, **When** the Asset suite is selected,
   **Then** only the requested focused suite executes and its exit status
   reflects that suite's result.
4. **Given** Windows, macOS, and Linux automation, **When** the feature is
   validated, **Then** identity, registry, dispatch, and lifecycle coverage
   passes without a visible window or graphics runtime.

### Edge Cases

- An identity input is empty, absolute, drive-qualified, UNC-like, contains
  parent traversal, has empty internal segments, or ends with a separator.
- Logical paths contain non-ASCII UTF-8 in composed or decomposed forms, mixed
  separators, repeated separators, case-only differences, dot segments, or
  reserved subresource delimiters.
- An asset type is empty, exceeds its byte limit, starts with a non-letter, or
  contains characters outside its case-sensitive ASCII symbolic grammar; a
  subresource name is empty, invalid UTF-8, delimiter-ambiguous, or extremely
  long.
- Two textual inputs canonicalize to the same typed identity.
- Two different identities or versions produce the same internal lookup-hasher
  value; full identity or version value comparison must still distinguish them.
  Two available digests with the same algorithm and digest bytes represent the
  same revision evidence because Feature 020 does not retain original payload
  bytes for cryptographic-collision disambiguation.
- A version record omits one or more hashes because the corresponding source,
  content, or cooked payload does not yet exist.
- A multi-output import contains duplicate subresource identities, depends on
  another output in the same atomic batch, or refers to a required target that
  is not registered until a later batch.
- Required dependencies form self-cycles or multi-record cycles; optional soft
  references form cycles but are not declared as required load dependencies.
- Updating or removing an asset leaves reverse dependents that must remain
  inspectable.
- A resolver reports not-found, inaccessible, malformed, or transient storage
  failure without leaking platform-specific error text into stable categories.
- A source locator has an empty or invalid scheme, invalid UTF-8 locator text,
  forbidden control characters, or exceeds its component limit.
- No importer claims a source, one importer wins, or multiple importers report
  equal strongest claims.
- Importer hint filtering leaves more than 64 eligible candidates; dispatch
  fails before any candidate receives probe bytes.
- A content probe receives empty, truncated, malformed, or oversized input.
- Extensions are registered twice, unregistered during idle lifecycle, or
  referenced after their registration token is released.
- An extension is unregistered after selection but before or during callback
  execution, while one or more execution leases remain active.
- Concurrent readers query identity, type, source, and dependency indexes while
  registration, replacement, or removal batches are serialized and published.
- Registry queries and dumps run against an empty registry.

## Architecture & Design Constraints *(mandatory)*

- **Asset Boundary**: Production code in the Asset layer MUST depend only on
  Core. It MUST NOT include or link RHI, Renderer, Application, Backend, Tools,
  platform graphics APIs, or editor services.
- **CPU-Side Ownership**: This feature owns identity, version, metadata,
  dependency, registry, and processing contracts only. It MUST NOT create or
  retain GPU resources.
- **Identity Independence**: Asset identity MUST be independent from physical
  storage location, source contents, cooked contents, target platform, and
  process-local registration order.
- **Storage Independence**: Filesystem, package, memory, and future remote
  storage are resolver concerns. Registry records MUST NOT make one storage
  provider mandatory.
- **Responsibility Separation**: Identity normalization, registry ownership,
  dependency validation, resolution, import dispatch, loading, and cooking
  contracts MUST remain separable responsibilities rather than one manager
  object.
- **Registry Concurrency**: Registry queries MUST permit concurrent readers.
  Mutations MUST be serialized internally and published atomically so readers
  never observe partially updated records or indexes. Fully concurrent writes
  are outside this feature.
- **Extension Lifetime**: Registration ownership and in-flight execution
  lifetime MUST be distinct. Unregistration MUST remove future eligibility
  immediately without invalidating already-issued execution leases.
- **Future Compatibility**: Contracts MUST support one source producing
  multiple typed assets and must leave room for Features 021-026 without
  embedding image, texture, material, mesh, shader, manifest, asynchronous
  request, or residency policy in Feature 020.
- **Naming Conventions**: Public code design MUST adhere to PascalCase,
  Unreal Engine-style naming conventions.
- **Cross-Platform Compatibility**: Canonical identity and deterministic
  dispatch behavior MUST be identical on Windows, macOS, and Linux regardless
  of native path syntax or filesystem case behavior.
- **Automated Cross-Platform Validation**: Windows, macOS, and Linux automation
  MUST build and run the focused Asset tests. No native graphics execution is
  required because this feature has no RHI or backend responsibility.
- **Review Debt Gate**: The focused suite-selection debt recorded as
  `CR001-B09-F003` MUST be resolved as part of this feature before broad Asset
  validation is added. The native deferred-session debt
  `CR001-B09-F005` is unaffected and MUST NOT be expanded by this feature.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST represent every asset identity as an explicit
  asset type, a canonical logical path, and an optional named subresource.
  Asset type text MUST use the case-sensitive ASCII grammar
  `[A-Za-z][A-Za-z0-9_.-]*`, MUST be no more than 255 bytes, and MUST NOT be
  Unicode-normalized or case-folded.
- **FR-002**: Logical identities MUST be platform-independent, relative,
  case-sensitive, valid UTF-8 values normalized to Unicode NFC with `/` as the
  canonical separator; accepted alternate separators and redundant
  current-directory syntax MUST normalize deterministically, while invalid
  UTF-8, absolute paths, drive or network roots, parent traversal, empty final
  paths, and ambiguous delimiter use MUST be rejected.
- **FR-003**: Canonicalization MUST be idempotent and MUST produce the same
  identity text, equality result, ordering, and stable diagnostic form on
  Windows, macOS, and Linux; canonically equivalent Unicode text MUST produce
  the same identity.
- **FR-004**: Asset type and subresource components MUST participate in identity
  equality and ordering; distinct types or subresources at the same logical
  path MUST remain distinct.
- **FR-005**: The system MUST provide typed soft references that preserve an
  asset identity without requiring the target to be registered or loaded and
  reject use as an incompatible asset type.
- **FR-006**: The system MUST represent source, imported-content, and cooked
  revision hashes separately from stable identity, including an explicit
  unavailable state for revisions not yet produced.
- **FR-007**: Version equality and inspection MUST compare every version field
  and full digest value and MUST remain correct when tests deliberately force a
  collision in an internal lookup hasher. Two available digests with the same
  algorithm and digest bytes are equal revision evidence; detecting a genuine
  cryptographic collision between different source payloads is outside this
  feature.
- **FR-008**: Asset metadata MUST identify the asset, version, originating
  logical source, producer identity and version, descriptive attributes, and
  declared dependencies without embedding a runtime payload. Source schemes
  MUST canonicalize to lowercase ASCII matching `[a-z][a-z0-9+.-]*` with a
  63-byte limit. Resolver-private locator text MUST be valid NFC UTF-8, remain
  case-sensitive, reject NUL/control characters, and be 1–1,024 bytes.
- **FR-009**: Each dependency MUST identify its target, dependency role, whether
  it is required for producing or using the referring asset, and whether its
  target is currently `Resolved` or `Unresolved`.
- **FR-010**: Required dependencies whose targets are not yet registered MUST
  be accepted in an explicit `Unresolved` state and MUST transition
  deterministically to `Resolved` when their targets are registered. Known
  self-dependencies and required-dependency cycles MUST be rejected. A soft
  reference that is not declared as a required dependency MAY form a cycle and
  MUST NOT be treated as a required load edge.
- **FR-011**: The in-memory registry MUST support atomic registration,
  replacement, and removal of one or more metadata records, including all
  subresources emitted from one source.
- **FR-012**: A failed batch operation MUST leave identity records, source
  indexes, type indexes, and forward and reverse dependency indexes unchanged.
- **FR-013**: Re-registering canonically value-equivalent metadata MUST be
  idempotent. Equivalence MUST compare identity, version, canonical source,
  producer identity/version, attributes as an insertion-order-independent
  key/value set, and dependency declarations sorted by target, role, and
  strength; it MUST NOT compare object layout, container insertion order, or
  registry-derived resolution state. Conflicting metadata for an existing
  identity MUST require an explicit replacement operation.
- **FR-014**: Registry queries MUST support exact identity, asset type,
  originating source, direct dependency, and reverse-dependent lookup without
  loading content.
- **FR-015**: All multi-result queries and inspection dumps MUST use ascending
  canonical asset identity as their final ordering key and MUST remain stable
  across equivalent operation histories.
- **FR-016**: Removing or replacing a dependency target MUST atomically update
  affected reverse edges to `Unresolved`; restoring the target MUST resolve
  those edges again. Registry completeness validation MUST fail while any
  required dependency is unresolved and MUST identify every incomplete
  referring asset and target.
- **FR-017**: Resolver registration MUST declare a stable participant identity,
  priority, supported logical domain or scheme, and lifecycle token; resolution
  MUST return a stable result category and a source descriptor without changing
  the asset identity. Resolver outcomes MUST map not-found to `NotFound`,
  inaccessible/permission failures to `AccessDenied`, malformed source data to
  `MalformedSource`, and retryable storage failures to `TransientFailure`.
- **FR-018**: Resolver selection MUST choose only a unique highest-priority
  eligible participant. Equal highest-priority candidates MUST return
  `AmbiguousResolver`; participant identity MUST provide stable candidate
  ordering and diagnostics but MUST NOT silently break the functional tie.
- **FR-019**: Importer registration MUST declare stable participant identity,
  supported format hints, probe limits, and producer version. Registration MAY
  retain more than 64 importers because capabilities can be disjoint.
  Participant identity MUST be a case-sensitive ASCII token matching
  `[A-Za-z][A-Za-z0-9_.-]*` with a 127-byte limit. Producer version MUST match
  `[A-Za-z0-9][A-Za-z0-9_.+-]*` with a 64-byte limit. Neither value is
  Unicode-normalized or case-folded, and participant ordering is bytewise.
- **FR-020**: Importer selection MUST first restrict candidates by declared
  hints when available. If that filtering leaves more than 64 eligible
  candidates, selection MUST return `CapacityExceeded` before reading source
  bytes or invoking a probe. Otherwise it MUST compare content-probe confidence
  using at most 64 KiB per candidate and select only a unique strongest valid
  candidate; no match and strongest-match ties MUST produce distinct failures.
- **FR-021**: Import discovery MUST allow one source to declare multiple typed
  output assets with stable subresource names, versions, metadata, and
  dependencies before any registry mutation occurs.
- **FR-022**: Loader and cooker extension contracts MUST accept typed identity,
  version, metadata, dependency, settings, and target-profile context as
  applicable and return explicit success, unsupported, invalid-input,
  dependency-failure, and processing-failure outcomes without requiring a
  concrete format in this feature.
- **FR-023**: Extension registration and unregistration MUST reject duplicate
  participant identities. Releasing a registration token MUST atomically
  prevent new selection while allowing already-selected requests holding valid
  execution leases to finish; the extension instance MUST remain alive until
  its final lease is released and MUST never receive a stale invocation.
- **FR-024**: Diagnostics MUST identify operation stage, stable result category,
  subject asset or participant, and actionable reason without native addresses,
  unstable iteration order, or platform-dependent error text in normalized
  output.
- **FR-025**: The feature MUST expose a public `FAssetInspection` API providing
  deterministic human-readable inspection for identities, digests, versions,
  metadata, dependency edges, registry contents, and registered extension
  capabilities.
- **FR-026**: The project test runner MUST support first-class focused selection
  of the Asset suite, resolving `CR001-B09-F003` without environment-variable
  skips or output filtering while preserving the existing all-suite default.
- **FR-027**: Automated tests MUST cover canonicalization, type safety, hash
  collisions, atomic registry mutation, dependency cycles, reverse lookup,
  deterministic queries, resolver selection, importer ambiguity, multi-output
  discovery, registration lifecycle, and failure diagnostics.
- **FR-028**: Windows, macOS, and Linux automation MUST build the Asset layer and
  run its focused deterministic tests while preserving all existing regression
  outcomes.
- **FR-029**: Concrete image/model/audio/font formats, payload decoding,
  persistent databases, editor discovery, asynchronous requests, hot reload,
  network storage, GPU resources, and residency MUST remain outside this
  feature.
- **FR-030**: Registry queries MUST be safe for concurrent readers. Registration,
  replacement, and removal batches MUST be serialized internally, and every
  reader MUST observe either the complete state before a batch or the complete
  state after it, including identity, source, type, and dependency indexes.

### Key Entities

- **Asset Identity**: Stable typed logical identity composed of a canonical
  logical path and optional subresource, independent from version and storage.
- **Asset Version**: Separate source, imported-content, and cooked revision
  evidence used for change detection rather than identity.
- **Asset Metadata**: Registry-owned description of identity, provenance,
  producer, version, attributes, and dependency declarations.
- **Asset Dependency**: A typed edge from one asset to another with a declared
  role, required/soft semantics, and current `Resolved` or `Unresolved` state.
- **Typed Soft Reference**: An unloaded reference that carries the expected
  asset type and stable identity.
- **Asset Registry**: Process-local metadata and dependency index supporting
  concurrent deterministic queries, serialized atomic mutation, and
  inspection.
- **Asset Resolver**: Registered participant that maps a logical request to a
  storage-independent source descriptor.
- **Asset Importer**: Registered participant that probes a source and discovers
  one or more typed asset outputs.
- **Asset Loader**: Contract for converting a selected asset description into
  its CPU-side runtime payload in later concrete-asset features.
- **Asset Cooker**: Contract for producing target-specific derived payloads and
  version evidence in later offline-tool features.
- **Extension Registration**: Scoped lifecycle record containing participant
  identity, capabilities, precedence, and validity.
- **Asset Diagnostic**: Stable stage, category, subject, participant, and
  actionable reason for an asset operation.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A corpus of at least 100 valid and invalid identity inputs,
  including at least 20 composed/decomposed Unicode-equivalent pairs, produces
  the same acceptance result and NFC canonical text on Windows, macOS, and
  Linux; every equivalent pair produces one identity, and a second
  canonicalization pass changes 0 accepted identities.
- **SC-002**: Across at least 20 repeated runs, equivalent identity sets produce
  identical equality, ordering, typed-reference validation, and normalized
  inspection output in all runs.
- **SC-003**: Atomic registration tests containing at least 50 assets, 10 source
  groups, and 100 dependency edges publish every valid record, preserve 100% of
  prior registry state after each injected conflict or known cycle, and retain
  every missing required target as an explicitly inspectable `Unresolved` edge.
- **SC-004**: Exact, type, source, direct-dependency, and reverse-dependent
  queries return the expected identities in canonical order for 100% of the
  validation corpus before and after accepted replacements, removals, and
  unresolved-to-resolved-to-unresolved dependency transitions; completeness
  validation identifies 100% of unresolved required edges.
- **SC-005**: Dispatch matrices covering no match, one match, lower-confidence
  alternatives, priority resolution, strongest-match ties, and 65 eligible
  importers select the expected participant, ambiguity result, or
  `CapacityExceeded` result in 100% of cases regardless of registration order;
  the over-capacity case invokes zero probes.
- **SC-006**: At least one synthetic source discovers 8 or more mixed-type
  subresources with stable identities and dependencies, and 20 repeated
  discoveries produce identical normalized output.
- **SC-007**: Every tested invalid path, type mismatch, duplicate identity,
  dependency error, resolver not-found/access-denied/malformed/transient
  failure, importer ambiguity, stale registration, and processing failure
  reports the correct stage, subject, and stable category in its first
  actionable diagnostic.
- **SC-008**: Focused Asset-suite selection executes only Asset tests, and an
  in-process test of the reusable suite registry returns status `1` when a
  test-only callback registered under the canonical `asset` name reports a
  failure. The production CLI exposes no failure-injection option, and the
  existing all-suite invocation behavior remains unchanged.
- **SC-009**: Automated architecture validation finds zero production
  dependencies from Asset to RHI, Renderer, Application, Backend, Tools, editor
  services, or a graphics API.
- **SC-010**: Windows, macOS, and Linux automated jobs build the feature, pass
  all focused Asset tests, and retain a 100% pass rate for the existing
  regression suite without requiring graphics hardware.
- **SC-011**: Under at least 8 concurrent readers and 100 serialized mutation
  batches containing registration, replacement, and removal operations, every
  observed registry snapshot satisfies all identity and index invariants, all
  operations complete without a hang or crash, and no reader observes a partial
  batch.
- **SC-012**: In at least 100 repeated races between extension dispatch and
  unregistration, every request selected before unregistration completes
  through a valid lease, zero requests selected afterward invoke the extension,
  and the instance is destroyed exactly once after its final lease is released.

## Assumptions

- The feature serves engine developers and later engine subsystems; it does not
  expose an end-user asset browser.
- Logical paths are manually editable project-relative identifiers, not native
  filesystem paths or URLs. They normalize valid UTF-8 to NFC while preserving
  case so identity does not vary with host filesystem normalization or
  case-folding behavior.
- Asset type names and subresource names use validated stable symbolic names;
  asset type text follows the fixed case-sensitive ASCII grammar in FR-001,
  while subresource text follows the UTF-8/NFC and delimiter rules defined for
  identity components.
- Revision evidence uses algorithm-tagged 32-byte SHA-256 digests and lowercase
  hexadecimal inspection as defined by FR-007. Producer versions use the
  case-sensitive ASCII token grammar and length limit defined by FR-019.
- Registry state is process-local and in-memory. Persistence, manifests, and an
  asset database belong to later roadmap phases.
- The registry owns reader/writer synchronization. Callers do not need external
  locking for queries or individual mutation batches, while coordinating the
  semantic order of competing writers remains a caller responsibility.
- Multiple resolvers may legitimately coexist for disjoint mounts or explicit
  development, cooked-package, plugin, and test-overlay priorities; overlapping
  equal-priority claims are configuration errors rather than fallback chains.
- Dispatch obtains an execution lease before invoking an extension.
  Unregistration is non-blocking with respect to already-started requests and
  does not imply cancellation; later asynchronous cancellation policy belongs
  to Feature 026.
- Required dependency edges model production or runtime prerequisites and must
  be acyclic. Typed soft references alone do not imply required dependencies.
- Resolver, importer, loader, and cooker behavior in this feature is validated
  with synthetic participants; concrete PNG, JPEG, HDR, KTX2, glTF, material,
  mesh, and shader payloads are introduced by Features 021-025.
- Import probing is side-effect-free and bounded after hint filtering to at
  most 64 candidates and 64 KiB per candidate. Exceeding the candidate bound
  fails before any probe callback.
- Digest values are revision evidence rather than proof that original payload
  bytes differ. Feature 020 tests collision-safe container behavior by forcing
  internal lookup-hasher collisions; it does not attempt to manufacture or
  distinguish two payloads with the same SHA-256 digest.
- Cross-platform CI is required because canonical path and ordering behavior are
  platform-sensitive; native graphics validation is not relevant to this
  Core-only layer.
