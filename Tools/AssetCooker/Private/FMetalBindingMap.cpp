#include "FMetalBindingMap.h"

#include <algorithm>
#include <limits>
#include <new>
#include <set>
#include <tuple>

namespace Stoner::AssetCooker::Private
{
namespace
{

bool IsStage(Asset::EShaderStage Stage)
{
    return Stage == Asset::EShaderStage::Vertex ||
        Stage == Asset::EShaderStage::Fragment ||
        Stage == Asset::EShaderStage::Compute;
}

bool IsVisible(
    const Asset::FShaderInterfaceBinding& Binding,
    Asset::EShaderStage Stage)
{
    return std::find(
        Binding.Visibility.begin(), Binding.Visibility.end(), Stage) !=
        Binding.Visibility.end();
}

Asset::EShaderNativeResourceClass NativeClass(
    Asset::EShaderResourceKind Kind)
{
    switch (Kind)
    {
    case Asset::EShaderResourceKind::UniformBuffer:
    case Asset::EShaderResourceKind::StorageBuffer:
        return Asset::EShaderNativeResourceClass::Buffer;
    case Asset::EShaderResourceKind::SampledTexture:
    case Asset::EShaderResourceKind::StorageTexture:
        return Asset::EShaderNativeResourceClass::Texture;
    case Asset::EShaderResourceKind::Sampler:
        return Asset::EShaderNativeResourceClass::Sampler;
    case Asset::EShaderResourceKind::CombinedTextureSampler:
        return static_cast<Asset::EShaderNativeResourceClass>(255);
    }
    return static_cast<Asset::EShaderNativeResourceClass>(255);
}

Core::uint32 LimitFor(
    const FMetalBindingLimits& Limits,
    Asset::EShaderNativeResourceClass Class)
{
    switch (Class)
    {
    case Asset::EShaderNativeResourceClass::Buffer:
        return Limits.MaxBufferBindings;
    case Asset::EShaderNativeResourceClass::Texture:
        return Limits.MaxTextureBindings;
    case Asset::EShaderNativeResourceClass::Sampler:
        return Limits.MaxSamplerBindings;
    }
    return 0;
}

bool IsReserved(
    const Asset::FShaderNativeBindingEvidence& Evidence,
    Asset::EShaderNativeResourceClass Class,
    Core::uint32 Index)
{
    return std::any_of(
        Evidence.ReservedRanges.begin(), Evidence.ReservedRanges.end(),
        [Class, Index](const auto& Range)
        {
            return Range.NativeClass == Class &&
                Index >= Range.FirstIndex &&
                static_cast<Core::uint64>(Index) <
                    static_cast<Core::uint64>(Range.FirstIndex) + Range.Count;
        });
}

bool Allocate(
    const Asset::FShaderNativeBindingEvidence& Evidence,
    const FMetalBindingLimits& Limits,
    Asset::EShaderNativeResourceClass Class,
    Core::uint32& Cursor,
    Core::uint32& Out)
{
    const Core::uint32 Limit = LimitFor(Limits, Class);
    while (Cursor < Limit && IsReserved(Evidence, Class, Cursor)) ++Cursor;
    if (Cursor >= Limit) return false;
    Out = Cursor++;
    return true;
}

} // namespace

bool FMetalBindingLimits::IsValid() const noexcept
{
    return MaxBufferBindings >= 2 && MaxBufferBindings <= 31 &&
        MaxTextureBindings > 0 && MaxTextureBindings <= 128 &&
        MaxSamplerBindings > 0 && MaxSamplerBindings <= 16;
}

Asset::EAssetResult BuildMetalBindingMap(
    const FMetalBindingMapRequest& Request,
    Asset::FShaderNativeBindingEvidence& OutEvidence) noexcept
{
    OutEvidence = {};
    if (!IsStage(Request.Stage) || !Request.Limits.IsValid())
        return Asset::EAssetResult::InvalidInput;
    try
    {
        const auto Fail = [&OutEvidence](Asset::EAssetResult Result)
        {
            OutEvidence = {};
            return Result;
        };
        OutEvidence.PolicyVersion = Core::FString("metal-direct-binding-v1");
        if (Request.Stage == Asset::EShaderStage::Vertex)
        {
            OutEvidence.ReservedRanges.push_back({
                Request.Stage,
                Asset::EShaderNativeResourceClass::Buffer,
                0,
                1,
                Core::FString("vertex-input")});
        }
        OutEvidence.ReservedRanges.push_back({
            Request.Stage,
            Asset::EShaderNativeResourceClass::Buffer,
            Request.Limits.MaxBufferBindings - 1,
            1,
            Core::FString("constant-data")});
        std::sort(
            OutEvidence.ReservedRanges.begin(),
            OutEvidence.ReservedRanges.end(),
            [](const auto& Left, const auto& Right)
            {
                return std::tie(
                           Left.Stage, Left.NativeClass, Left.FirstIndex) <
                    std::tie(
                           Right.Stage, Right.NativeClass, Right.FirstIndex);
            });
        OutEvidence.LimitSnapshot = {
            {Request.Stage, Asset::EShaderNativeResourceClass::Buffer,
             Request.Limits.MaxBufferBindings},
            {Request.Stage, Asset::EShaderNativeResourceClass::Texture,
             Request.Limits.MaxTextureBindings},
            {Request.Stage, Asset::EShaderNativeResourceClass::Sampler,
             Request.Limits.MaxSamplerBindings}};

        Core::TArray<const Asset::FShaderInterfaceBinding*> Bindings;
        std::set<std::pair<Core::uint32, Core::uint32>> SourceBindings;
        for (const auto& Binding : Request.InterfaceBindings)
        {
            if (!IsVisible(Binding, Request.Stage)) continue;
            if (Binding.ArrayCount == 0 ||
                !SourceBindings.insert(
                    {Binding.SetIndex, Binding.BindingIndex}).second)
                return Fail(Asset::EAssetResult::Conflict);
            Bindings.push_back(&Binding);
        }
        std::sort(
            Bindings.begin(), Bindings.end(),
            [](const auto* Left, const auto* Right)
            {
                return std::tie(
                           Left->SetIndex, Left->BindingIndex, Left->Kind) <
                    std::tie(
                           Right->SetIndex, Right->BindingIndex, Right->Kind);
            });

        Core::uint32 BufferCursor = 0;
        Core::uint32 TextureCursor = 0;
        Core::uint32 SamplerCursor = 0;
        for (const auto* Binding : Bindings)
        {
            if (Binding->Kind ==
                Asset::EShaderResourceKind::CombinedTextureSampler)
            {
                if (Binding->ArrayCount > Request.Limits.MaxTextureBindings ||
                    Binding->ArrayCount > Request.Limits.MaxSamplerBindings)
                    return Fail(Asset::EAssetResult::CapacityExceeded);
                for (Core::uint32 Element = 0;
                    Element < Binding->ArrayCount; ++Element)
                {
                    Core::uint32 TextureIndex = 0;
                    Core::uint32 SamplerIndex = 0;
                    if (!Allocate(
                            OutEvidence, Request.Limits,
                            Asset::EShaderNativeResourceClass::Texture,
                            TextureCursor, TextureIndex) ||
                        !Allocate(
                            OutEvidence, Request.Limits,
                            Asset::EShaderNativeResourceClass::Sampler,
                            SamplerCursor, SamplerIndex))
                        return Fail(Asset::EAssetResult::CapacityExceeded);
                    OutEvidence.Entries.push_back({
                        Request.Stage, Binding->SetIndex,
                        Binding->BindingIndex, Binding->Kind, Element,
                        Asset::EShaderNativeResourceClass::Texture,
                        TextureIndex});
                    OutEvidence.Entries.push_back({
                        Request.Stage, Binding->SetIndex,
                        Binding->BindingIndex, Binding->Kind, Element,
                        Asset::EShaderNativeResourceClass::Sampler,
                        SamplerIndex});
                }
                continue;
            }
            const auto Class = NativeClass(Binding->Kind);
            Core::uint32* Cursor = nullptr;
            switch (Class)
            {
            case Asset::EShaderNativeResourceClass::Buffer:
                Cursor = &BufferCursor;
                break;
            case Asset::EShaderNativeResourceClass::Texture:
                Cursor = &TextureCursor;
                break;
            case Asset::EShaderNativeResourceClass::Sampler:
                Cursor = &SamplerCursor;
                break;
            }
            if (!Cursor || Binding->ArrayCount > LimitFor(Request.Limits, Class))
                return Fail(Asset::EAssetResult::CapacityExceeded);
            for (Core::uint32 Element = 0; Element < Binding->ArrayCount; ++Element)
            {
                Core::uint32 NativeIndex = 0;
                if (!Allocate(
                        OutEvidence, Request.Limits, Class, *Cursor,
                        NativeIndex))
                {
                    return Fail(Asset::EAssetResult::CapacityExceeded);
                }
                OutEvidence.Entries.push_back({
                    Request.Stage,
                    Binding->SetIndex,
                    Binding->BindingIndex,
                    Binding->Kind,
                    Element,
                    Class,
                    NativeIndex});
            }
        }
        const Asset::EAssetResult Result =
            Asset::FinalizeShaderNativeBindingEvidence(OutEvidence);
        if (Result != Asset::EAssetResult::Success) OutEvidence = {};
        return Result;
    }
    catch (const std::bad_alloc&)
    {
        OutEvidence = {};
        return Asset::EAssetResult::CapacityExceeded;
    }
    catch (const std::length_error&)
    {
        OutEvidence = {};
        return Asset::EAssetResult::CapacityExceeded;
    }
}

} // namespace Stoner::AssetCooker::Private
