# Contract: Render Graph Foundation

## Scope

This contract describes the Renderer-layer public behavior for declaring, compiling, inspecting, and executing render graphs. It is a C++ library contract, not a network or CLI contract.

## Public Types

### `FRenderGraph`

Owns a graph instance. It exposes lifecycle operations for compile, execute, reset, invalidation, diagnostics, and debug dumping.

Required behavior:

- Reject execution before a successful compile.
- Reject executable compile when no output exists and no side-effect-preserving pass exists.
- Preserve deterministic compiled metadata for repeated identical declarations.
- Stop execution on first pass failure.
- Reset without leaking compiled plans or callbacks.

### `FRenderGraphBuilder`

Declares graph resources, passes, access declarations, graph outputs, imports, and execution callbacks.

Required behavior:

- Return stable pass/resource handles.
- Reject handles from other graph instances.
- Prevent mutation while the graph is compiled or executing.
- Allow side-effect-preserving pass declaration.

### `FRenderGraphResourceDesc`

Describes virtual resource identity and compatibility inputs.

Required behavior:

- Cover texture and buffer descriptions needed by existing RHI contracts.
- Identify transient, imported, and exported ownership.
- Carry initial availability state for imported resources.
- Carry alias policy and debug name.

### `FRenderGraphPassDesc`

Describes pass identity and scheduling inputs.

Required behavior:

- Include pass name, pass type, side-effect preservation, and resource access declarations.
- Support graphics, compute, copy-like utility, and side-effect work.
- Carry execution callback metadata without exposing backend API details.

### `FCompiledRenderGraph`

Represents compilation output.

Required behavior:

- Expose scheduled passes and culled passes.
- Expose resource lifetimes.
- Expose aliasing eligibility/rejection reasons.
- Expose inspectable transition plan.
- Expose validation diagnostics.

### `FRenderGraphExecutionContext`

Provides the pass callback with graph-resolved resources and an RHI-facing command context.

Required behavior:

- Provide only declared resources to a pass.
- Provide caller-supplied imported resources after validation.
- Provide transient resources resolved during graph execution.
- Prevent access to resources that were not declared by the pass.

## Compile Contract

Input:

- Declared graph passes and virtual resources.
- Resource access declarations.
- Graph outputs and side-effect pass flags.
- Imported resource availability declarations.

Output:

- Success with compiled schedule, lifetimes, aliasing eligibility, transition plan, and diagnostics.
- Failure with deterministic diagnostics and no executable schedule.

Required validation:

- Detect dependency cycles.
- Detect read-before-write transient usage.
- Detect invalid writes to read-only imports.
- Detect incompatible usage/layout declarations.
- Detect zero-output executable graphs without side-effect-preserving passes.
- Preserve stable declaration-order tie breaking for independent passes.

## Execution Contract

Preconditions:

- Graph has compiled successfully.
- Required imported resources are provided and valid.
- RHI-facing resource creation and command recording context are available.

Required behavior:

- Resolve transient resources during execution.
- Validate imported resources before invoking pass callbacks.
- Emit transition records matching the compiled transition plan.
- Invoke pass callbacks in compiled schedule order.
- Stop immediately if transient resolution, transition emission, or pass execution fails.
- Report failing pass name/index and failure category when pass execution fails.

## Aliasing Contract

Required behavior:

- Report eligible non-overlapping compatible transient resources.
- Report rejection reasons for overlapping lifetimes, incompatible descriptions, imported resources, exported external ownership, and explicit no-alias policy.
- Do not reuse backing storage for eligible resources during this feature phase.

## Debug Dump Contract

Required behavior:

- Produce deterministic text output for successful and failed graph states.
- Include pass list, resource list, dependency edges, culling summary, lifetime summary, aliasing summary, transition summary, and diagnostics.
- Include invalid pass/resource context when compilation fails.

## Boundary Rules

- Public Renderer graph contracts must not include Vulkan, Metal, DX12, OpenGL, GLES, WebGL, or platform-window concepts.
- Material systems, shader permutations, concrete forward/deferred passes, scene graph integration, visible presentation, async compute overlap, and real backing-storage alias reuse are out of scope.
