# Feature Specification: Asset Core, Identity & Registry

**Feature Branch**: `codex/020-asset-core`  
**Created**: 2026-07-28  
**Status**: Draft  
**Input**: User description: "基于 Roadmap 2.1 的下一阶段，建立 Asset Core、Identity 与 Registry 基础"

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
3. **Given** an asset with required dependencies, **When** a dependency is
   missing or would create a required-dependency cycle, **Then** the affected
   batch is rejected with an actionable diagnostic.
4. **Given** a registered dependency graph, **When** an asset is queried or
   removed, **Then** direct dependencies, reverse dependents, and resulting
   invalid references are reported consistently.

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
   resolution is requested, **Then** one deterministic eligible resolver is
   selected or ambiguity is rejected.
2. **Given** candidate importers selected by declared format support, **When**
   bounded content probing is performed, **Then** the strongest unique valid
   match is selected and tied matches fail rather than depending on registration
   order.
3. **Given** one source that emits several typed assets, **When** import
   discovery succeeds, **Then** every output has a stable subresource identity,
   metadata, version information, and declared dependencies.
4. **Given** an extension that fails, is unregistered, or outlives its valid
   owner, **When** a later request is made, **Then** no stale callback is invoked
   and the failure identifies the stage and participant.

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
- Logical paths contain non-ASCII UTF-8, mixed separators, repeated separators,
  case-only differences, dot segments, or reserved subresource delimiters.
- An asset type or subresource name is empty, invalid, or extremely long.
- Two textual inputs canonicalize to the same typed identity.
- Two different identities or versions produce the same underlying hash value.
- A version record omits one or more hashes because the corresponding source,
  content, or cooked payload does not yet exist.
- A multi-output import contains duplicate subresource identities or depends on
  another output in the same atomic batch.
- Required dependencies form self-cycles or multi-record cycles; optional soft
  references form cycles but are not declared as required load dependencies.
- Updating or removing an asset leaves reverse dependents that must remain
  inspectable.
- A resolver reports not-found, inaccessible, malformed, or transient storage
  failure without leaking platform-specific error text into stable categories.
- No importer claims a source, one importer wins, or multiple importers report
  equal strongest claims.
- A content probe receives empty, truncated, malformed, or oversized input.
- Extensions are registered twice, unregistered during idle lifecycle, or
  referenced after their registration token is released.
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
- **FR-002**: Logical identities MUST be platform-independent, relative,
  case-sensitive, UTF-8-preserving values with `/` as the canonical separator;
  accepted alternate separators and redundant current-directory syntax MUST
  normalize deterministically, while absolute paths, drive or network roots,
  parent traversal, empty final paths, and ambiguous delimiter use MUST be
  rejected.
- **FR-003**: Canonicalization MUST be idempotent and MUST produce the same
  identity text, equality result, ordering, and stable diagnostic form on
  Windows, macOS, and Linux.
- **FR-004**: Asset type and subresource components MUST participate in identity
  equality and ordering; distinct types or subresources at the same logical
  path MUST remain distinct.
- **FR-005**: The system MUST provide typed soft references that preserve an
  asset identity without requiring the target to be registered or loaded and
  reject use as an incompatible asset type.
- **FR-006**: The system MUST represent source, imported-content, and cooked
  revision hashes separately from stable identity, including an explicit
  unavailable state for revisions not yet produced.
- **FR-007**: Version equality and inspection MUST compare full version data and
  MUST remain correct even when a hash collision is deliberately introduced by
  tests.
- **FR-008**: Asset metadata MUST identify the asset, version, originating
  logical source, producer identity and version, descriptive attributes, and
  declared dependencies without embedding a runtime payload.
- **FR-009**: Each dependency MUST identify its target, dependency role, and
  whether it is required for producing or using the referring asset.
- **FR-010**: Required dependency relationships MUST reject missing targets,
  self-dependencies, and cycles. A soft reference that is not declared as a
  required dependency MAY form a cycle and MUST NOT be treated as a required
  load edge.
- **FR-011**: The in-memory registry MUST support atomic registration,
  replacement, and removal of one or more metadata records, including all
  subresources emitted from one source.
- **FR-012**: A failed batch operation MUST leave identity records, source
  indexes, type indexes, and forward and reverse dependency indexes unchanged.
- **FR-013**: Re-registering byte-for-byte equivalent metadata MUST be
  idempotent; conflicting metadata for an existing identity MUST require an
  explicit replacement operation and MUST never depend on insertion order.
- **FR-014**: Registry queries MUST support exact identity, asset type,
  originating source, direct dependency, and reverse-dependent lookup without
  loading content.
- **FR-015**: All multi-result queries and inspection dumps MUST use ascending
  canonical asset identity as their final ordering key and MUST remain stable
  across equivalent operation histories.
- **FR-016**: Removing or replacing a record referenced by another record MUST
  either preserve dependency validity atomically or reject the operation and
  identify every blocking dependent.
- **FR-017**: Resolver registration MUST declare a stable participant identity,
  priority, supported logical domain or scheme, and lifecycle token; resolution
  MUST return a stable result category and a source descriptor without changing
  the asset identity.
- **FR-018**: Resolver selection MUST be deterministic from declared
  eligibility, priority, and participant identity; an unresolved strongest tie
  MUST fail as ambiguous rather than use registration order.
- **FR-019**: Importer registration MUST declare stable participant identity,
  supported format hints, probe limits, and producer version.
- **FR-020**: Importer selection MUST first restrict candidates by declared
  hints when available, then compare bounded content-probe confidence, and
  select only a unique strongest valid candidate; no match and strongest-match
  ties MUST produce distinct failures.
- **FR-021**: Import discovery MUST allow one source to declare multiple typed
  output assets with stable subresource names, versions, metadata, and
  dependencies before any registry mutation occurs.
- **FR-022**: Loader and cooker extension contracts MUST accept typed identity,
  version, metadata, dependency, settings, and target-profile context as
  applicable and return explicit success, unsupported, invalid-input,
  dependency-failure, and processing-failure outcomes without requiring a
  concrete format in this feature.
- **FR-023**: Extension registration and unregistration MUST reject duplicate
  participant identities, prevent invocation after registration lifetime ends,
  and leave dispatch state valid after failures.
- **FR-024**: Diagnostics MUST identify operation stage, stable result category,
  subject asset or participant, and actionable reason without native addresses,
  unstable iteration order, or platform-dependent error text in normalized
  output.
- **FR-025**: The feature MUST provide deterministic human-readable inspection
  for identities, metadata, dependency edges, registry contents, and registered
  extension capabilities.
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

### Key Entities

- **Asset Identity**: Stable typed logical identity composed of a canonical
  logical path and optional subresource, independent from version and storage.
- **Asset Version**: Separate source, imported-content, and cooked revision
  evidence used for change detection rather than identity.
- **Asset Metadata**: Registry-owned description of identity, provenance,
  producer, version, attributes, and dependency declarations.
- **Asset Dependency**: A typed edge from one asset to another with a declared
  role and required/soft semantics.
- **Typed Soft Reference**: An unloaded reference that carries the expected
  asset type and stable identity.
- **Asset Registry**: Process-local metadata and dependency index supporting
  atomic mutation, deterministic query, and inspection.
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

- **SC-001**: A corpus of at least 100 valid and invalid identity inputs produces
  the same acceptance result and canonical text on Windows, macOS, and Linux,
  and a second canonicalization pass changes 0 accepted identities.
- **SC-002**: Across at least 20 repeated runs, equivalent identity sets produce
  identical equality, ordering, typed-reference validation, and normalized
  inspection output in all runs.
- **SC-003**: Atomic registration tests containing at least 50 assets, 10 source
  groups, and 100 dependency edges either publish every valid record or preserve
  100% of the prior registry state after each injected conflict, missing
  dependency, or cycle.
- **SC-004**: Exact, type, source, direct-dependency, and reverse-dependent
  queries return the expected identities in canonical order for 100% of the
  validation corpus before and after accepted replacements and removals.
- **SC-005**: Dispatch matrices covering no match, one match, lower-confidence
  alternatives, priority resolution, and strongest-match ties select the
  expected participant or ambiguity result in 100% of cases regardless of
  registration order.
- **SC-006**: At least one synthetic source discovers 8 or more mixed-type
  subresources with stable identities and dependencies, and 20 repeated
  discoveries produce identical normalized output.
- **SC-007**: Every tested invalid path, type mismatch, duplicate identity,
  dependency error, resolver failure, importer ambiguity, stale registration,
  and processing failure reports the correct stage, subject, and stable category
  in its first actionable diagnostic.
- **SC-008**: Focused Asset-suite selection executes only Asset tests, returns a
  failing status for an injected Asset failure, and leaves the existing
  all-suite invocation behavior unchanged.
- **SC-009**: Automated architecture validation finds zero production
  dependencies from Asset to RHI, Renderer, Application, Backend, Tools, editor
  services, or a graphics API.
- **SC-010**: Windows, macOS, and Linux automated jobs build the feature, pass
  all focused Asset tests, and retain a 100% pass rate for the existing
  regression suite without requiring graphics hardware.

## Assumptions

- The feature serves engine developers and later engine subsystems; it does not
  expose an end-user asset browser.
- Logical paths are manually editable project-relative identifiers, not native
  filesystem paths or URLs. They preserve UTF-8 bytes and case so identity does
  not vary with host filesystem behavior.
- Asset type names and subresource names use validated stable symbolic names;
  the plan may choose their concrete representation without changing identity
  semantics.
- Hash algorithms, digest width, textual encoding, and producer-version
  representation will be selected during planning, provided collision-safe
  equality and deterministic inspection requirements remain satisfied.
- Registry state is process-local and in-memory. Persistence, manifests, and an
  asset database belong to later roadmap phases.
- Required dependency edges model production or runtime prerequisites and must
  be acyclic. Typed soft references alone do not imply required dependencies.
- Resolver, importer, loader, and cooker behavior in this feature is validated
  with synthetic participants; concrete PNG, JPEG, HDR, KTX2, glTF, material,
  mesh, and shader payloads are introduced by Features 021-025.
- Import probing is bounded and side-effect-free. Planning will define concrete
  byte and candidate limits that satisfy the deterministic dispatch contract.
- Cross-platform CI is required because canonical path and ordering behavior are
  platform-sensitive; native graphics validation is not relevant to this
  Core-only layer.

