#pragma once

#include "Asset/FStaticMeshAsset.h"
#include "Renderer/FStaticMeshRealization.h"
#include "RHI/RHIMinimal.h"

#include <cstring>
#include <optional>

namespace Stoner::Tests::StaticMesh
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace Stoner::RHI;

inline FAssetId MakeId(
    const char* Type,
    const char* Path,
    const char* Subresource)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString(Type), FString(Path),
        std::optional<FString>(FString(Subresource)), Id);
    return Id;
}

inline FStaticMeshBounds MakeBounds(float Offset)
{
    FStaticMeshBounds Bounds;
    Bounds.Box = FBox(
        FVector3(Offset, 0.0f, 0.0f),
        FVector3(Offset + 1.0f, 1.0f, 0.0f));
    Bounds.Sphere = FSphere(
        FVector3(Offset + 0.5f, 0.5f, 0.0f), 0.75f);
    return Bounds;
}

inline FStaticMeshPrimitive MakePrimitive(
    const char* Key,
    float Offset,
    Core::uint32 SourceIndex)
{
    FStaticMeshPrimitive Primitive;
    Primitive.StableKey = FString(Key);
    Primitive.SourcePrimitiveIndex = SourceIndex;
    Primitive.Vertices.Positions = {
        FVector3(Offset, 0.0f, 0.0f),
        FVector3(Offset + 1.0f, 0.0f, 0.0f),
        FVector3(Offset, 1.0f, 0.0f)};
    Primitive.Vertices.Normals = {
        FVector3::UnitZ(), FVector3::UnitZ(), FVector3::UnitZ()};
    Primitive.Vertices.Tangents = {
        FVector4(1.0f, 0.0f, 0.0f, 1.0f),
        FVector4(1.0f, 0.0f, 0.0f, 1.0f),
        FVector4(1.0f, 0.0f, 0.0f, 1.0f)};
    Primitive.Vertices.TexCoords[0] = {
        FVector2(0.0f, 0.0f),
        FVector2(1.0f, 0.0f),
        FVector2(0.0f, 1.0f)};
    Primitive.Vertices.TexCoords[1] = {
        FVector2(0.25f, 0.25f),
        FVector2(0.75f, 0.25f),
        FVector2(0.25f, 0.75f)};
    (void)FStaticMeshIndexData::Create({0, 1, 2}, Primitive.Indices);
    Primitive.LocalBounds = MakeBounds(Offset);
    return Primitive;
}

inline TSharedPtr<const FStaticMeshAsset> MakeAsset(bool bTwoPrimitives = true)
{
    FStaticMeshAssetDesc Desc;
    Desc.Id = MakeId("StaticMesh", "Tests/Renderer/Mesh", "mesh.0");
    Desc.ImportProfileDigest = FAssetDigest::FromBytes(TArray<uint8>{0x24});
    const FAssetId MaterialId =
        MakeId("Material", "Tests/Renderer/Mesh", "material.0");
    FStaticMeshMaterialSlot Slot;
    Slot.StableKey = FString("material.0");
    (void)TSoftAssetRef<FMaterialAsset>::Create(MaterialId, Slot.Material);
    Desc.MaterialSlots.push_back(std::move(Slot));
    Desc.Dependencies.push_back({
        MaterialId,
        EAssetDependencyRole::Runtime,
        EAssetDependencyStrength::Required,
        EAssetDependencyResolution::Unresolved});
    Desc.SourceManifest.push_back({
        MakeId("StaticMeshSource", "Tests/Renderer/Mesh", "source"),
        {}, EAssetSourceRole::Source});
    Desc.Primitives.push_back(MakePrimitive("primitive.0", 0.0f, 0));
    if (bTwoPrimitives)
    {
        Desc.Primitives.push_back(MakePrimitive("primitive.1", 2.0f, 1));
        Desc.Bounds.Box = FBox(
            FVector3(0.0f, 0.0f, 0.0f),
            FVector3(3.0f, 1.0f, 0.0f));
        Desc.Bounds.Sphere = FSphere(FVector3(1.5f, 0.5f, 0.0f), 1.6f);
    }
    else
    {
        Desc.Bounds = MakeBounds(0.0f);
    }
    FStaticMeshAsset Asset;
    if (FStaticMeshAsset::CreateValidated(std::move(Desc), Asset) !=
        EAssetResult::Success)
    {
        return nullptr;
    }
    return MakeShared<const FStaticMeshAsset>(std::move(Asset));
}

class FBuffer final : public IRHIBuffer
{
public:
    explicit FBuffer(FRHIBufferDesc InDesc) : Desc_(InDesc) {}

    [[nodiscard]] const FRHIBufferDesc& GetDesc() const noexcept override
    {
        return Desc_;
    }
    [[nodiscard]] uint64 GetSizeInBytes() const noexcept override
    {
        return Desc_.SizeInBytes;
    }
    [[nodiscard]] ERHIBufferUsage GetUsage() const noexcept override
    {
        return Desc_.Usage;
    }
    [[nodiscard]] ERHIResourceLifecycleState
    GetLifecycleState() const noexcept override
    {
        return State;
    }
    ERHIResult Invalidate() override
    {
        State = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

    TArray<uint8> Bytes;
    bool bInvalidateAfterUpload = false;

private:
    FRHIBufferDesc Desc_;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FDevice final : public IRHIDevice
{
public:
    [[nodiscard]] ERHIDeviceState GetState() const noexcept override
    {
        return bActive ? ERHIDeviceState::Active : ERHIDeviceState::Shutdown;
    }
    [[nodiscard]] const FRHIDeviceCapabilities&
    GetCapabilities() const noexcept override { return Capabilities; }
    [[nodiscard]] bool IsActive() const noexcept override { return bActive; }
    ERHIResult Shutdown() override
    {
        bActive = false;
        return ERHIResult::Success;
    }

    TRHIObjectResult<IRHIBuffer> CreateBuffer(
        const FRHIBufferDesc& Desc) override
    {
        if (++CreateCalls == FailCreateCall)
        {
            return {ERHIResult::Failed, nullptr};
        }
        auto Value = MakeShared<FBuffer>(Desc);
        Value->bInvalidateAfterUpload = bInvalidateAfterUpload &&
            CreateCalls == 2;
        Created.push_back(Value);
        return {ERHIResult::Success, Value};
    }

    ERHIResult UploadBuffer(
        const TSharedPtr<IRHIBuffer>& Buffer,
        const FRHIBufferUploadDesc& Upload) override
    {
        if (++UploadCalls == FailUploadCall)
        {
            return ERHIResult::Failed;
        }
        auto Value = std::dynamic_pointer_cast<FBuffer>(Buffer);
        if (!Value || !IsValidRHIBufferUploadDesc(Value->GetDesc(), Upload))
        {
            return ERHIResult::InvalidState;
        }
        Value->Bytes.resize(static_cast<usize>(Upload.DataSizeBytes));
        std::memcpy(Value->Bytes.data(), Upload.Data,
            static_cast<usize>(Upload.DataSizeBytes));
        if (Value->bInvalidateAfterUpload)
        {
            (void)Value->Invalidate();
        }
        return ERHIResult::Success;
    }

#define STONER_UNSUPPORTED_FACTORY(Name, Type, Arguments) \
    TRHIObjectResult<Type> Name Arguments override \
    { return {ERHIResult::Unsupported, nullptr}; }
    STONER_UNSUPPORTED_FACTORY(CreateCommandQueue, IRHICommandQueue,
        (ERHIQueueType))
    STONER_UNSUPPORTED_FACTORY(CreateCommandBuffer, IRHICommandBuffer,
        (ERHIQueueType))
    STONER_UNSUPPORTED_FACTORY(CreateFence, IRHIFence, (bool))
    STONER_UNSUPPORTED_FACTORY(CreateSemaphore, IRHISemaphore, ())
    STONER_UNSUPPORTED_FACTORY(CreateSwapchain, IRHISwapchain, (uint32))
    STONER_UNSUPPORTED_FACTORY(CreateTexture, IRHITexture,
        (const FRHITextureDesc&))
    STONER_UNSUPPORTED_FACTORY(CreateSampler, IRHISampler,
        (const FRHISamplerDesc&))
    STONER_UNSUPPORTED_FACTORY(CreateShaderModule, IRHIShaderModule,
        (const FRHIShaderModuleDesc&))
    STONER_UNSUPPORTED_FACTORY(CreatePipelineLayout, IRHIPipelineLayout,
        (const FRHIPipelineLayoutDesc&))
    STONER_UNSUPPORTED_FACTORY(CreateDescriptorSet, IRHIDescriptorSet,
        (const TSharedPtr<IRHIPipelineLayout>&, uint32))
    STONER_UNSUPPORTED_FACTORY(CreateGraphicsPipeline, IRHIGraphicsPipeline,
        (const FRHIGraphicsPipelineDesc&))
    STONER_UNSUPPORTED_FACTORY(CreateComputePipeline, IRHIComputePipeline,
        (const FRHIComputePipelineDesc&))
    STONER_UNSUPPORTED_FACTORY(CreateRenderPass, IRHIRenderPass,
        (const FRHIRenderPassDesc&))
    STONER_UNSUPPORTED_FACTORY(CreateFramebuffer, IRHIFramebuffer,
        (const FRHIFramebufferDesc&))
#undef STONER_UNSUPPORTED_FACTORY

    bool bActive = true;
    int CreateCalls = 0;
    int UploadCalls = 0;
    int FailCreateCall = -1;
    int FailUploadCall = -1;
    bool bInvalidateAfterUpload = false;
    TArray<TSharedPtr<FBuffer>> Created;

private:
    FRHIDeviceCapabilities Capabilities;
};

inline bool AllCreatedInvalid(const FDevice& Device)
{
    return !Device.Created.empty() &&
        std::all_of(Device.Created.begin(), Device.Created.end(),
            [](const auto& Buffer)
            {
                return Buffer->GetLifecycleState() ==
                    ERHIResourceLifecycleState::Invalidated;
            });
}

} // namespace Stoner::Tests::StaticMesh
