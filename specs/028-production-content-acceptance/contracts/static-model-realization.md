# Renderer Static-Model Realization Contract

## Public Surface

```cpp
namespace Stoner::Renderer
{
struct FStaticModelRealizationLimits;
struct FStaticModelRealizationDependencies;
struct FStaticModelRealizationRequest;
struct FStaticModelRealizationInspection;

class FStaticModelRenderSnapshot;

class FStaticModelRealizer
{
public:
    static RHI::ERHIResult Realize(
        const FStaticModelRealizationRequest& Request,
        Core::TSharedPtr<const FStaticModelRenderSnapshot>& OutSnapshot,
        FStaticModelRealizationInspection& OutInspection);
};
}
```

Exact member spelling may follow established local APIs, but ownership and
behavior below are normative.

## Request Contract

- Receives a live `IRHIDevice` and immutable `FStaticModelAsset`.
- Receives explicit typed dependency maps for every referenced static mesh,
  material/instance, selected shader payload/snapshot, and KTX2 texture artifact.
- Receives exact target profile/capability evidence and bounded count/byte limits.
- Does not own or call `FAssetManager`, resolver/importer, AssetCooker, Demo,
  Application, Vulkan, or Metal APIs.
- Rejects missing, mismatched, ambiguous, stale, or unreferenced conflicting
  dependencies before resource commit.

## Transaction Contract

1. Validate the complete CPU dependency set and compute deterministic node,
   primitive, material, and resource realization order.
2. Reuse shared dependencies by AssetId plus version/target evidence.
3. Create mesh buffers, transcode/realize KTX2 textures, convert material/shader
   snapshots, and prepare descriptor/pipeline bindings using existing Renderer
   and RHI contracts.
4. Hold every resource in a private transaction. No caller-visible draw or
   snapshot exists during preparation.
5. On success, atomically transfer ownership into one immutable snapshot.
6. On failure/cancellation, release in reverse dependency order exactly once,
   clear `OutSnapshot`, and return the first stable failure.

## Snapshot Contract

The snapshot contains:

- root AssetId/version and process-local snapshot generation;
- parent-before-child node transforms;
- deterministic primitive draw records with mesh/index range, material binding,
  bounds, and stable source identity;
- immutable Renderer material/shader snapshots;
- all required RHI buffer, texture, descriptor, and pipeline ownership;
- bounded inspection counts and stable IDs, never native pointers.

It is safe to read after source Asset handles are released only if its Renderer
snapshots own all copied CPU state required for drawing. RHI ownership ends when
the final snapshot owner releases it. A snapshot generation is never reused.

## Observable Results

- `Success`: complete usable snapshot; all inspection counts agree.
- `InvalidInput`: invalid CPU payload, incomplete dependency set, limits, or
  incompatible target evidence.
- `Unsupported`: valid content requires unsupported RHI capability or format.
- Backend/RHI allocation, upload, descriptor, pipeline, submission,
  cancellation, or device-loss result: no snapshot and complete rollback.

## Required Tests

- Multi-node, multi-primitive, multi-material successful commit.
- Shared mesh/texture creates one compatible GPU resource.
- Missing/wrong-type/stale dependency rejected before GPU work.
- Failure injection at every allocation/upload/descriptor/pipeline step.
- Reverse-order exactly-once rollback and zero public partial state.
- Destroy/recreate generation safety and stale snapshot rejection by composition.
- Renderer/Application architecture scan contains no Vulkan/Metal includes or calls.
