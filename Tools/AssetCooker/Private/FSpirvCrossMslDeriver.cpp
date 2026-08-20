#include "FSpirvCrossMslDeriver.h"

#include "spirv_msl.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <new>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace Stoner::AssetCooker::Private
{
namespace
{

spv::ExecutionModel ExecutionModel(Asset::EShaderStage Stage)
{
    switch (Stage)
    {
    case Asset::EShaderStage::Vertex:
        return spv::ExecutionModelVertex;
    case Asset::EShaderStage::Fragment:
        return spv::ExecutionModelFragment;
    case Asset::EShaderStage::Compute:
        return spv::ExecutionModelGLCompute;
    }
    return spv::ExecutionModelMax;
}

bool IsVisible(
    const Asset::FShaderInterfaceBinding& Binding,
    Asset::EShaderStage Stage)
{
    return std::find(
        Binding.Visibility.begin(), Binding.Visibility.end(), Stage) !=
        Binding.Visibility.end();
}

void U32(Core::TArray<Core::uint8>& Out, Core::uint32 Value)
{
    Out.push_back(static_cast<Core::uint8>(Value));
    Out.push_back(static_cast<Core::uint8>(Value >> 8U));
    Out.push_back(static_cast<Core::uint8>(Value >> 16U));
    Out.push_back(static_cast<Core::uint8>(Value >> 24U));
}

Asset::FAssetDigest InterfaceDigest(
    Asset::EShaderStage Stage,
    std::span<const Asset::FShaderInterfaceBinding> Bindings)
{
    Core::TArray<const Asset::FShaderInterfaceBinding*> Sorted;
    for (const auto& Binding : Bindings)
        if (IsVisible(Binding, Stage)) Sorted.push_back(&Binding);
    std::sort(
        Sorted.begin(), Sorted.end(),
        [](const auto* Left, const auto* Right)
        {
            return std::tie(
                       Left->SetIndex, Left->BindingIndex, Left->Kind,
                       Left->ArrayCount) <
                std::tie(
                       Right->SetIndex, Right->BindingIndex, Right->Kind,
                       Right->ArrayCount);
        });
    Core::TArray<Core::uint8> Bytes;
    U32(Bytes, static_cast<Core::uint32>(Stage));
    U32(Bytes, static_cast<Core::uint32>(Sorted.size()));
    for (const auto* Binding : Sorted)
    {
        U32(Bytes, Binding->SetIndex);
        U32(Bytes, Binding->BindingIndex);
        U32(Bytes, static_cast<Core::uint32>(Binding->Kind));
        U32(Bytes, Binding->ArrayCount);
    }
    return Asset::FAssetDigest::FromBytes(Bytes);
}

Core::uint32 ArrayCount(
    const spirv_cross::CompilerMSL& Compiler,
    const spirv_cross::Resource& Resource)
{
    const auto& Type = Compiler.get_type(Resource.type_id);
    if (Type.array.empty()) return 1;
    if (Type.array.size() != 1 || Type.array_size_literal.empty() ||
        !Type.array_size_literal.front())
        return 0;
    return Type.array.front();
}

struct FReflectedBinding
{
    Core::uint32 Set = 0;
    Core::uint32 Binding = 0;
    Asset::EShaderResourceKind Kind =
        Asset::EShaderResourceKind::UniformBuffer;
    Core::uint32 Count = 1;
};

bool AddResources(
    const spirv_cross::CompilerMSL& Compiler,
    const spirv_cross::SmallVector<spirv_cross::Resource>& Resources,
    Asset::EShaderResourceKind Kind,
    Core::TArray<FReflectedBinding>& Out)
{
    for (const auto& Resource : Resources)
    {
        FReflectedBinding Binding;
        Binding.Set = Compiler.get_decoration(
            Resource.id, spv::DecorationDescriptorSet);
        Binding.Binding = Compiler.get_decoration(
            Resource.id, spv::DecorationBinding);
        Binding.Kind = Kind;
        Binding.Count = ArrayCount(Compiler, Resource);
        if (Binding.Count == 0) return false;
        Out.push_back(Binding);
    }
    return true;
}

const Asset::FShaderNativeBindingEntry* FindNativeEntry(
    const Asset::FShaderNativeBindingEvidence& Evidence,
    const FReflectedBinding& Reflected,
    Asset::EShaderNativeResourceClass NativeClass)
{
    const auto Found = std::find_if(
        Evidence.Entries.begin(), Evidence.Entries.end(),
        [&Reflected, NativeClass](const auto& Entry)
        {
            return Entry.SetIndex == Reflected.Set &&
                Entry.BindingIndex == Reflected.Binding &&
                Entry.DescriptorType == Reflected.Kind &&
                Entry.NativeClass == NativeClass &&
                Entry.ArrayElement == 0;
        });
    return Found == Evidence.Entries.end() ? nullptr : &*Found;
}

bool ConfigureBindings(
    spirv_cross::CompilerMSL& Compiler,
    spv::ExecutionModel Model,
    const Core::TArray<FReflectedBinding>& Reflected,
    const Asset::FShaderNativeBindingEvidence& Evidence)
{
    for (const auto& Resource : Reflected)
    {
        const bool bCombined = Resource.Kind ==
            Asset::EShaderResourceKind::CombinedTextureSampler;
        const auto* Entry = FindNativeEntry(
            Evidence, Resource,
            bCombined ? Asset::EShaderNativeResourceClass::Texture
                      : (Resource.Kind == Asset::EShaderResourceKind::Sampler
                            ? Asset::EShaderNativeResourceClass::Sampler
                            : (Resource.Kind == Asset::EShaderResourceKind::SampledTexture ||
                                      Resource.Kind == Asset::EShaderResourceKind::StorageTexture
                                  ? Asset::EShaderNativeResourceClass::Texture
                                  : Asset::EShaderNativeResourceClass::Buffer)));
        if (!Entry) return false;
        spirv_cross::MSLResourceBinding Binding;
        Binding.stage = Model;
        Binding.desc_set = Resource.Set;
        Binding.binding = Resource.Binding;
        Binding.count = Resource.Count;
        switch (Entry->NativeClass)
        {
        case Asset::EShaderNativeResourceClass::Buffer:
            Binding.msl_buffer = Entry->NativeIndex;
            break;
        case Asset::EShaderNativeResourceClass::Texture:
            Binding.msl_texture = Entry->NativeIndex;
            break;
        case Asset::EShaderNativeResourceClass::Sampler:
            Binding.msl_sampler = Entry->NativeIndex;
            break;
        }
        if (bCombined)
        {
            const auto* SamplerEntry = FindNativeEntry(
                Evidence, Resource,
                Asset::EShaderNativeResourceClass::Sampler);
            if (!SamplerEntry) return false;
            Binding.msl_sampler = SamplerEntry->NativeIndex;
        }
        Compiler.add_msl_resource_binding(Binding);
    }
    return true;
}

bool InterfaceMatches(
    Asset::EShaderStage Stage,
    std::span<const Asset::FShaderInterfaceBinding> Declared,
    Core::TArray<FReflectedBinding> Reflected)
{
    Core::TArray<FReflectedBinding> Expected;
    for (const auto& Binding : Declared)
    {
        if (IsVisible(Binding, Stage))
            Expected.push_back({
                Binding.SetIndex, Binding.BindingIndex, Binding.Kind,
                Binding.ArrayCount});
    }
    const auto Less = [](const auto& Left, const auto& Right)
    {
        return std::tie(
                   Left.Set, Left.Binding, Left.Kind, Left.Count) <
            std::tie(Right.Set, Right.Binding, Right.Kind, Right.Count);
    };
    std::sort(Expected.begin(), Expected.end(), Less);
    std::sort(Reflected.begin(), Reflected.end(), Less);
    return Expected.size() == Reflected.size() &&
        std::equal(
            Expected.begin(), Expected.end(), Reflected.begin(),
            [](const auto& Left, const auto& Right)
            {
                return Left.Set == Right.Set &&
                    Left.Binding == Right.Binding &&
                    Left.Kind == Right.Kind && Left.Count == Right.Count;
            });
}

} // namespace

bool FSpirvCrossMslResult::IsValid() const noexcept
{
    return !NormalizedMsl.IsEmpty() && SpirvDigest.IsAvailable() &&
        InterfaceDigest.IsAvailable() && OptionsDigest.IsAvailable() &&
        NormalizedMslDigest.IsAvailable() &&
        BindingEvidence.Validate() == Asset::EAssetResult::Success;
}

Core::FString MakeMetalNativeEntryPoint(
    const Core::FString& LogicalEntryPoint)
{
    return LogicalEntryPoint.IsEmpty()
        ? Core::FString()
        : Core::FString(
              "stoner_" + LogicalEntryPoint.ToStdString());
}

Asset::EAssetResult NormalizeMetalShaderSource(
    std::string_view Source,
    Core::FString& OutNormalized) noexcept
{
    OutNormalized.Clear();
    if (Source.empty() || Source.find('\0') != std::string_view::npos)
        return Asset::EAssetResult::InvalidInput;
    try
    {
        std::string Normalized;
        std::size_t Begin = 0;
        while (Begin < Source.size())
        {
            std::size_t End = Source.find_first_of("\r\n", Begin);
            if (End == std::string_view::npos) End = Source.size();
            std::string_view Line = Source.substr(Begin, End - Begin);
            while (!Line.empty() &&
                (Line.back() == ' ' || Line.back() == '\t'))
                Line.remove_suffix(1);
            if (!Line.starts_with("// stoner-volatile:"))
            {
                Normalized.append(Line);
                Normalized.push_back('\n');
            }
            if (End == Source.size()) break;
            if (Source[End] == '\r' && End + 1 < Source.size() &&
                Source[End + 1] == '\n')
                ++End;
            Begin = End + 1;
        }
        if (Normalized.empty()) return Asset::EAssetResult::InvalidInput;
        OutNormalized = Core::FString(std::move(Normalized));
        return Asset::EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        return Asset::EAssetResult::CapacityExceeded;
    }
}

Asset::EAssetResult DeriveMetalShaderSource(
    const FSpirvCrossMslRequest& Request,
    FSpirvCrossMslResult& OutResult) noexcept
{
    OutResult = {};
    const spv::ExecutionModel Model = ExecutionModel(Request.Stage);
    if (Model == spv::ExecutionModelMax || Request.EntryPoint.IsEmpty() ||
        Request.SpirvBytes.size() < 20 ||
        Request.SpirvBytes.size() % sizeof(Core::uint32) != 0 ||
        !Request.BindingLimits.IsValid())
        return Asset::EAssetResult::InvalidInput;
    try
    {
        std::vector<Core::uint32> Words(
            Request.SpirvBytes.size() / sizeof(Core::uint32));
        std::memcpy(
            Words.data(), Request.SpirvBytes.data(),
            Request.SpirvBytes.size());
        spirv_cross::CompilerMSL Compiler(std::move(Words));
        Compiler.set_entry_point(Request.EntryPoint.ToStdString(), Model);
        if (Compiler.get_execution_model() != Model)
            return Asset::EAssetResult::DependencyMismatch;
        Compiler.rename_entry_point(
            Request.EntryPoint.ToStdString(),
            MakeMetalNativeEntryPoint(Request.EntryPoint).ToStdString(),
            Model);

        const auto Resources = Compiler.get_shader_resources();
        Core::TArray<FReflectedBinding> Reflected;
        if (!AddResources(
                Compiler, Resources.sampled_images,
                Asset::EShaderResourceKind::CombinedTextureSampler, Reflected) ||
            !AddResources(
                Compiler, Resources.uniform_buffers,
                Asset::EShaderResourceKind::UniformBuffer, Reflected) ||
            !AddResources(
                Compiler, Resources.storage_buffers,
                Asset::EShaderResourceKind::StorageBuffer, Reflected) ||
            !AddResources(
                Compiler, Resources.separate_images,
                Asset::EShaderResourceKind::SampledTexture, Reflected) ||
            !AddResources(
                Compiler, Resources.storage_images,
                Asset::EShaderResourceKind::StorageTexture, Reflected) ||
            !AddResources(
                Compiler, Resources.separate_samplers,
                Asset::EShaderResourceKind::Sampler, Reflected) ||
            !InterfaceMatches(
                Request.Stage, Request.InterfaceBindings, Reflected))
            return Asset::EAssetResult::DependencyMismatch;

        FMetalBindingMapRequest BindingRequest;
        BindingRequest.Stage = Request.Stage;
        BindingRequest.InterfaceBindings = Request.InterfaceBindings;
        BindingRequest.Limits = Request.BindingLimits;
        Asset::FShaderNativeBindingEvidence Evidence;
        const Asset::EAssetResult BindingResult =
            BuildMetalBindingMap(BindingRequest, Evidence);
        if (BindingResult != Asset::EAssetResult::Success)
            return BindingResult;
        if (!ConfigureBindings(Compiler, Model, Reflected, Evidence))
            return Asset::EAssetResult::DependencyMismatch;

        spirv_cross::CompilerMSL::Options Options;
        Options.platform = spirv_cross::CompilerMSL::Options::macOS;
        Options.set_msl_version(2, 4);
        Options.argument_buffers = false;
        Compiler.set_msl_options(Options);
        const std::string Generated = Compiler.compile();
        Core::FString Normalized;
        const Asset::EAssetResult Normalize =
            NormalizeMetalShaderSource(Generated, Normalized);
        if (Normalize != Asset::EAssetResult::Success) return Normalize;

        static constexpr std::string_view OptionsIdentity =
            "spirv-cross:a0fba56c34a6700f1724bf9b751da5b488a3775c;"
            "platform=macos;msl=2.4;argument-buffers=0;"
            "entry-point=stoner-prefix-v1;"
            "binding-policy=metal-direct-binding-v1";
        OutResult.NormalizedMsl = std::move(Normalized);
        OutResult.SpirvDigest =
            Asset::FAssetDigest::FromBytes(Request.SpirvBytes);
        OutResult.InterfaceDigest =
            InterfaceDigest(Request.Stage, Request.InterfaceBindings);
        OutResult.OptionsDigest = Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>(
                reinterpret_cast<const Core::uint8*>(OptionsIdentity.data()),
                OptionsIdentity.size()));
        OutResult.NormalizedMslDigest = Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>(
                reinterpret_cast<const Core::uint8*>(
                    OutResult.NormalizedMsl.View().data()),
                OutResult.NormalizedMsl.Len()));
        OutResult.BindingEvidence = std::move(Evidence);
        return OutResult.IsValid()
            ? Asset::EAssetResult::Success
            : Asset::EAssetResult::ProcessingFailure;
    }
    catch (const spirv_cross::CompilerError&)
    {
        OutResult = {};
        return Asset::EAssetResult::MalformedSource;
    }
    catch (const std::bad_alloc&)
    {
        OutResult = {};
        return Asset::EAssetResult::CapacityExceeded;
    }
    catch (const std::exception&)
    {
        OutResult = {};
        return Asset::EAssetResult::ProcessingFailure;
    }
}

} // namespace Stoner::AssetCooker::Private
