# Data Model: Render Graph Foundation

## Render Graph

**Purpose**: Owns declared passes, virtual resources, graph outputs, validation diagnostics, compiled schedules, and execution state.

**Key fields**:

- `Name`: Optional debug name for deterministic dumps.
- `Passes`: Ordered collection of declared render graph passes.
- `Resources`: Ordered collection of virtual resources.
- `Outputs`: Resource handles marked as graph outputs.
- `CompilationState`: Draft, Compiled, Failed, Executed, Invalidated.
- `CompiledSchedule`: Deterministic schedule produced after validation.
- `Diagnostics`: Validation, culling, transition, execution, and invalidation records.

**Validation rules**:

- A graph must be compiled before execution.
- Executable compilation requires at least one output unless side-effect-preserving passes exist.
- Reset or invalidation clears compiled schedules, resource plans, and callback state.

## Render Graph Builder

**Purpose**: Provides the declaration surface for passes, virtual resources, imports, outputs, and execution callbacks.

**Key fields**:

- `Graph`: Target graph under construction.
- `NextPassIndex`: Stable insertion-order index.
- `NextResourceIndex`: Stable resource handle index.

**Validation rules**:

- The builder cannot mutate a compiled or executing graph.
- Pass and resource handles must belong to the same graph instance.
- Duplicate output marking is idempotent and deterministic.

## Render Graph Pass

**Purpose**: Represents one scheduled unit of work.

**Key fields**:

- `Name`: Required deterministic debug name.
- `Index`: Stable declaration order.
- `Type`: Graphics, Compute, Copy, or SideEffect.
- `Accesses`: Resource access declarations.
- `ExecutionCallback`: Callback invoked during graph execution.
- `bPreserveForSideEffects`: True for work that must not be culled solely due to missing outputs.

**Validation rules**:

- Pass names may repeat only if diagnostics include stable indices; debug dumps identify passes by name and index.
- A pass with no accesses is culled unless marked side-effect-preserving.
- Execution failure stops the graph immediately and reports this pass.

## Render Graph Resource

**Purpose**: Represents a virtual texture, buffer, or external resource before or during execution.

**Key fields**:

- `Handle`: Stable resource handle.
- `Name`: Debug name.
- `Kind`: Buffer, Texture, or External.
- `Description`: Dimensions, element counts, format, usage intent, and layout requirements.
- `Ownership`: Transient, Imported, or Exported.
- `InitialState`: Required for imported resources.
- `AliasPolicy`: Eligible, Disabled, Imported, ExportedExternal, or Incompatible.
- `ResolvedObject`: Concrete RHI-facing resource available during execution.

**Validation rules**:

- Transient resources require a producing write before any read.
- Imported resources require caller-supplied concrete resources and external availability state.
- Read-only imported resources cannot be written by graph passes.
- Exported outputs must be produced or imported before use.

## Resource Access Declaration

**Purpose**: Describes how a pass uses a resource.

**Key fields**:

- `PassHandle`: Declaring pass.
- `ResourceHandle`: Referenced resource.
- `AccessType`: Read, Write, ReadWrite, Create, Import, Export, or Preserve.
- `RequiredUsage`: Abstract resource usage needed by the pass.
- `RequiredLayout`: Abstract layout/state needed by the pass.

**Validation rules**:

- Access declarations must reference existing pass and resource handles.
- Incompatible usage and layout combinations fail graph compilation.
- Multiple writes require an ordering path or deterministic conflict diagnostic.

## Compiled Graph Schedule

**Purpose**: Represents the executable graph after validation and culling.

**Key fields**:

- `ScheduledPasses`: Ordered pass list.
- `CulledPasses`: Passes removed because they do not affect outputs or side effects.
- `ResourceLifetimes`: First-use and last-use pass indices for each resource.
- `AliasingDecisions`: Eligibility and rejection records.
- `TransitionPlan`: Ordered resource transition records.

**Validation rules**:

- Schedule must be acyclic.
- Independent pass tie-breaking follows declaration order.
- Culling must preserve output producers and side-effect-preserving passes.

## Resource Lifetime

**Purpose**: Describes when a resource becomes live and when it can be released.

**Key fields**:

- `ResourceHandle`: Target resource.
- `FirstUsePassIndex`: First scheduled use.
- `LastUsePassIndex`: Last scheduled use.
- `bImported`: Whether lifetime begins outside the graph.
- `bExported`: Whether lifetime extends beyond graph execution.

**Validation rules**:

- First use must not be after last use.
- Imported resources are not transient alias candidates.
- Exported externally owned resources are not transient alias candidates.

## Aliasing Decision

**Purpose**: Records whether two resources are eligible to share backing storage in a future optimization phase.

**Key fields**:

- `FirstResource`: First resource handle.
- `SecondResource`: Second resource handle.
- `State`: Eligible or Rejected.
- `Reason`: NonOverlappingCompatible, OverlappingLifetime, IncompatibleDescription, ImportedResource, ExportedExternalOwnership, ExplicitNoAlias.

**Validation rules**:

- Eligible resources still receive separate backing storage in this phase.
- Imported and exported external resources are always rejected for aliasing.

## Transition Plan

**Purpose**: Records resource state changes required between scheduled passes.

**Key fields**:

- `BeforePassIndex`: Scheduled pass before the transition.
- `BeforeState`: Previous resource state.
- `AfterPassIndex`: Scheduled pass requiring the new state.
- `AfterState`: Required resource state.
- `ResourceHandle`: Resource being transitioned.
- `Reason`: ReadAfterWrite, WriteAfterRead, WriteAfterWrite, GraphicsToCompute, ComputeToGraphics.

**Validation rules**:

- Redundant transitions are omitted.
- Execution emits transition records matching the compiled plan.

## Graph Debug Dump

**Purpose**: Provides deterministic text inspection of graph declaration, validation, compilation, and execution decisions.

**Key fields**:

- `GraphName`
- `PassList`
- `ResourceList`
- `DependencyEdges`
- `CullingSummary`
- `LifetimeSummary`
- `AliasingSummary`
- `TransitionSummary`
- `Diagnostics`

**Validation rules**:

- Repeated dumps of the same graph state must be byte-stable.
- Failed compilation dumps include invalid pass or resource context.

## State Transitions

```text
Draft
  ├── Compile succeeds -> Compiled
  ├── Compile fails    -> Failed
  └── Reset            -> Draft

Compiled
  ├── Execute succeeds -> Executed
  ├── Execute fails    -> Failed
  ├── Reset            -> Draft
  └── Invalidate       -> Invalidated

Failed
  ├── Reset            -> Draft
  └── Invalidate       -> Invalidated

Executed
  ├── Reset            -> Draft
  └── Invalidate       -> Invalidated

Invalidated
  └── Reset            -> Draft
```
