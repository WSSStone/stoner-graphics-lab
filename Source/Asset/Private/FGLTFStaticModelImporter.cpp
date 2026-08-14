#include "FGLTFStaticModelImporter.h"

#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelImport.h"
#include "FCgltfDocument.h"
#include "FGLTFAccessorDecoder.h"
#include "FGLTFGeometryNormalizer.h"
#include "FGLTFDiagnostics.h"
#include "FGLTFDependencyResolver.h"
#include "FGLTFDefaultMaterial.h"
#include "FGLTFImageTextureBridge.h"
#include "FGLTFMaterialMapper.h"
#include "FGLTFPackageAssembler.h"
#include "FGLTFPackageIdentityPlanner.h"
#include "FGLTFPackageValidator.h"
#include "FGLTFStableKey.h"
#include "FStaticMeshBounds.h"
#include "FStaticMeshNormalGenerator.h"
#include "FStaticMeshTangentGenerator.h"

#include "cgltf/cgltf.h"

#include <algorithm>
#include <cctype>
#include <limits>
#include <optional>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

FAssetParticipantId ImporterId()
{
    FAssetParticipantId Id;
    (void)FAssetParticipantId::Create(Core::FString("stoner.gltf.cgltf"), Id);
    return Id;
}

FAssetProducerVersion ImporterVersion()
{
    FAssetProducerVersion Version;
    (void)FAssetProducerVersion::Create(
        Core::FString("cgltf-1.15+static-model-1"), Version);
    return Version;
}

Core::FString OptionalExtensionSummary(const cgltf_data& Data)
{
    Core::TArray<Core::FString> Extensions;
    Extensions.reserve(Data.extensions_used_count);
    for (cgltf_size Index = 0; Index < Data.extensions_used_count; ++Index)
        if (Data.extensions_used[Index] != nullptr)
            Extensions.emplace_back(Data.extensions_used[Index]);
    std::sort(Extensions.begin(), Extensions.end());
    std::string Text;
    for (Core::usize Index = 0; Index < Extensions.size(); ++Index)
    {
        if (Index != 0) Text.push_back(',');
        Text += Extensions[Index].ToStdString();
    }
    return Core::FString(std::move(Text));
}

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetStage Stage,
    EAssetResult Result,
    const FAssetImportRequest& Request,
    const char* Code,
    const char* Field)
{
    AppendGLTFDiagnostic(Diagnostics,
        std::numeric_limits<Core::uint32>::max(), Stage, Result,
        EAssetDiagnosticSeverity::Error, Core::FString(Code),
        ImporterId().ToString(), Request.Descriptor.Location.ToString(),
        Core::FString(Field), Core::FString("static model package rejected"));
}

std::optional<EGLTFComponentType> ConvertComponentType(cgltf_component_type Type)
{
    switch (Type)
    {
    case cgltf_component_type_r_8: return EGLTFComponentType::Int8;
    case cgltf_component_type_r_8u: return EGLTFComponentType::UInt8;
    case cgltf_component_type_r_16: return EGLTFComponentType::Int16;
    case cgltf_component_type_r_16u: return EGLTFComponentType::UInt16;
    case cgltf_component_type_r_32u: return EGLTFComponentType::UInt32;
    case cgltf_component_type_r_32f: return EGLTFComponentType::Float32;
    default: return std::nullopt;
    }
}

std::optional<EGLTFAccessorType> ConvertAccessorType(cgltf_type Type)
{
    switch (Type)
    {
    case cgltf_type_scalar: return EGLTFAccessorType::Scalar;
    case cgltf_type_vec2: return EGLTFAccessorType::Vec2;
    case cgltf_type_vec3: return EGLTFAccessorType::Vec3;
    case cgltf_type_vec4: return EGLTFAccessorType::Vec4;
    default: return std::nullopt;
    }
}

struct FBufferStorage
{
    Core::TArray<Core::TArray<Core::uint8>> Owned;
    Core::TArray<std::span<const Core::uint8>> Views;
};

EAssetResult BuildBufferStorage(
    const cgltf_data& Data,
    const FStaticModelImportProfile& Profile,
    const FAssetSourceLocator& MainSource,
    const Core::TSharedPtr<IAssetResolver>& Resolver,
    Core::uint64& InOutAggregateDependencyBytes,
    FBufferStorage& OutStorage)
{
    OutStorage = {};
    OutStorage.Owned.resize(Data.buffers_count);
    OutStorage.Views.resize(Data.buffers_count);
    bool UsedBinaryChunk = false;
    for (cgltf_size Index = 0; Index < Data.buffers_count; ++Index)
    {
        const cgltf_buffer& Buffer = Data.buffers[Index];
        if (Buffer.size > Profile.Limits.MaxSingleDependencyBytes)
        {
            return EAssetResult::CapacityExceeded;
        }
        if (Buffer.uri == nullptr)
        {
            if (UsedBinaryChunk || Data.bin == nullptr || Buffer.size > Data.bin_size)
            {
                return EAssetResult::MalformedSource;
            }
            OutStorage.Views[Index] = std::span<const Core::uint8>(
                static_cast<const Core::uint8*>(Data.bin), Buffer.size);
            UsedBinaryChunk = true;
        }
        else
        {
            const std::string_view Uri(Buffer.uri);
            if (!Uri.starts_with("data:"))
            {
                FGLTFResolvedDependency Dependency;
                const EAssetResult ResolveResult = ResolveGLTFDependency(
                    MainSource, Uri, Resolver,
                    Profile.Limits.MaxSingleDependencyBytes, Dependency);
                if (ResolveResult != EAssetResult::Success)
                    return ResolveResult;
                OutStorage.Owned[Index] = std::move(Dependency.Bytes);
            }
            const EAssetResult Result = Uri.starts_with("data:")
                ? DecodeGLTFDataUri(Uri, Profile.Limits.MaxSingleDependencyBytes,
                      OutStorage.Owned[Index])
                : EAssetResult::Success;
            if (Result != EAssetResult::Success ||
                OutStorage.Owned[Index].size() < Buffer.size)
            {
                return Result == EAssetResult::Success
                    ? EAssetResult::MalformedSource : Result;
            }
            OutStorage.Views[Index] = OutStorage.Owned[Index];
            if (OutStorage.Owned[Index].size() >
                    Profile.Limits.MaxAggregateDependencyBytes -
                        InOutAggregateDependencyBytes)
                return EAssetResult::CapacityExceeded;
            InOutAggregateDependencyBytes += OutStorage.Owned[Index].size();
        }
    }
    return EAssetResult::Success;
}

EAssetResult GetBufferViewBytes(
    const cgltf_data& Data,
    const FBufferStorage& Storage,
    const cgltf_buffer_view* View,
    std::span<const Core::uint8>& OutBytes)
{
    OutBytes = {};
    if (View == nullptr || View->buffer == nullptr ||
        View->buffer < Data.buffers ||
        View->buffer >= Data.buffers + Data.buffers_count)
    {
        return EAssetResult::MalformedSource;
    }
    const cgltf_size BufferIndex =
        static_cast<cgltf_size>(View->buffer - Data.buffers);
    const std::span<const Core::uint8> Buffer = Storage.Views[BufferIndex];
    if (View->offset > Buffer.size() || View->size > Buffer.size() - View->offset)
    {
        return EAssetResult::MalformedSource;
    }
    OutBytes = Buffer.subspan(
        static_cast<Core::usize>(View->offset),
        static_cast<Core::usize>(View->size));
    return EAssetResult::Success;
}

EAssetResult MakeAccessorSource(
    const cgltf_data& Data,
    const FBufferStorage& Storage,
    const cgltf_accessor& Accessor,
    FGLTFAccessorSource& OutSource)
{
    OutSource = {};
    const auto ComponentType = ConvertComponentType(Accessor.component_type);
    const auto AccessorType = ConvertAccessorType(Accessor.type);
    if (!ComponentType || !AccessorType ||
        Accessor.count > std::numeric_limits<Core::uint32>::max())
    {
        return EAssetResult::MalformedSource;
    }
    OutSource.ComponentType = *ComponentType;
    OutSource.Type = *AccessorType;
    OutSource.Count = static_cast<Core::uint32>(Accessor.count);
    OutSource.bNormalized = Accessor.normalized != 0;
    OutSource.bHasBaseBufferView = Accessor.buffer_view != nullptr;
    if (Accessor.buffer_view != nullptr)
    {
        EAssetResult Result = GetBufferViewBytes(
            Data, Storage, Accessor.buffer_view, OutSource.BaseBytes);
        if (Result != EAssetResult::Success)
        {
            return EAssetResult::MalformedSource;
        }
        OutSource.ByteOffset = Accessor.offset;
        OutSource.ByteStride = Accessor.buffer_view->stride;
    }
    if (Accessor.is_sparse)
    {
        if (Accessor.sparse.count > std::numeric_limits<Core::uint32>::max())
        {
            return EAssetResult::CapacityExceeded;
        }
        FGLTFSparseAccessorSource Sparse;
        Sparse.Count = static_cast<Core::uint32>(Accessor.sparse.count);
        const auto SparseIndexType =
            ConvertComponentType(Accessor.sparse.indices_component_type);
        if (!SparseIndexType ||
            GetBufferViewBytes(Data, Storage,
                Accessor.sparse.indices_buffer_view, Sparse.Indices) !=
                EAssetResult::Success ||
            GetBufferViewBytes(Data, Storage,
                Accessor.sparse.values_buffer_view, Sparse.Values) !=
                EAssetResult::Success)
        {
            return EAssetResult::MalformedSource;
        }
        Sparse.IndexComponentType = *SparseIndexType;
        Sparse.IndicesByteOffset = Accessor.sparse.indices_byte_offset;
        Sparse.ValuesByteOffset = Accessor.sparse.values_byte_offset;
        OutSource.Sparse = Sparse;
    }
    return EAssetResult::Success;
}

EAssetResult DecodeAttribute(
    const cgltf_data& Data,
    const FBufferStorage& Storage,
    const cgltf_accessor* Accessor,
    EGLTFAccessorType ExpectedType,
    Core::uint32 MaximumVertices,
    FGLTFDecodedAccessor& OutDecoded)
{
    if (Accessor == nullptr)
    {
        return EAssetResult::NotFound;
    }
    FGLTFAccessorSource Source;
    EAssetResult Result = MakeAccessorSource(Data, Storage, *Accessor, Source);
    if (Result != EAssetResult::Success || Source.Type != ExpectedType)
    {
        return EAssetResult::MalformedSource;
    }
    return DecodeGLTFAccessorToFloat(Source, MaximumVertices, OutDecoded);
}

const cgltf_accessor* FindAttribute(
    const cgltf_primitive& Primitive,
    cgltf_attribute_type Type,
    cgltf_int Set = 0)
{
    const cgltf_accessor* Found = nullptr;
    for (cgltf_size Index = 0; Index < Primitive.attributes_count; ++Index)
    {
        const cgltf_attribute& Attribute = Primitive.attributes[Index];
        if (Attribute.type == Type && Attribute.index == Set)
        {
            if (Found != nullptr) return nullptr;
            Found = Attribute.data;
        }
    }
    return Found;
}

EAssetResult ValidateMaterialTexCoords(const cgltf_primitive& Primitive)
{
    if (Primitive.material == nullptr) return EAssetResult::Success;
    const cgltf_material& Material = *Primitive.material;
    const cgltf_texture_view* Views[] = {
        &Material.pbr_metallic_roughness.base_color_texture,
        &Material.pbr_metallic_roughness.metallic_roughness_texture,
        &Material.normal_texture,
        &Material.occlusion_texture,
        &Material.emissive_texture};
    for (const cgltf_texture_view* View : Views)
    {
        if (View->texture == nullptr) continue;
        if (View->texcoord < 0 || View->texcoord > 1 ||
            FindAttribute(Primitive, cgltf_attribute_type_texcoord,
                View->texcoord) == nullptr)
            return EAssetResult::MalformedSource;
    }
    return EAssetResult::Success;
}

EAssetResult AccountDecodedGeometry(
    const FStaticMeshPrimitive& Primitive,
    Core::uint64 MaximumBytes,
    Core::uint64& InOutBytes)
{
    const auto CheckedAdd = [&InOutBytes, MaximumBytes](Core::uint64 Bytes)
    {
        if (Bytes > MaximumBytes - InOutBytes) return false;
        InOutBytes += Bytes;
        return true;
    };
    const auto& Vertices = Primitive.Vertices;
    if (!CheckedAdd(Vertices.Positions.size() * sizeof(Core::FVector3)) ||
        !CheckedAdd(Vertices.Normals.size() * sizeof(Core::FVector3)) ||
        !CheckedAdd(Vertices.Tangents.size() * sizeof(Core::FVector4)))
        return EAssetResult::CapacityExceeded;
    for (const auto& TexCoords : Vertices.TexCoords)
        if (!CheckedAdd(TexCoords.size() * sizeof(Core::FVector2)))
            return EAssetResult::CapacityExceeded;
    const Core::uint64 IndexStride =
        Primitive.Indices.Uses16BitIndices()
        ? sizeof(Core::uint16) : sizeof(Core::uint32);
    return CheckedAdd(Primitive.Indices.GetIndexCount() * IndexStride)
        ? EAssetResult::Success : EAssetResult::CapacityExceeded;
}

void CopyVec2(const FGLTFDecodedAccessor& Source, Core::TArray<Core::FVector2>& Out)
{
    Out.reserve(Source.ElementCount);
    for (Core::uint32 Index = 0; Index < Source.ElementCount; ++Index)
    {
        Out.emplace_back(Source.Values[Index * 2], Source.Values[Index * 2 + 1]);
    }
}

void CopyVec3(const FGLTFDecodedAccessor& Source, Core::TArray<Core::FVector3>& Out)
{
    Out.reserve(Source.ElementCount);
    for (Core::uint32 Index = 0; Index < Source.ElementCount; ++Index)
    {
        Out.emplace_back(
            Source.Values[Index * 3], Source.Values[Index * 3 + 1],
            Source.Values[Index * 3 + 2]);
    }
}

void CopyVec4(const FGLTFDecodedAccessor& Source, Core::TArray<Core::FVector4>& Out)
{
    Out.reserve(Source.ElementCount);
    for (Core::uint32 Index = 0; Index < Source.ElementCount; ++Index)
    {
        Out.emplace_back(
            Source.Values[Index * 4], Source.Values[Index * 4 + 1],
            Source.Values[Index * 4 + 2], Source.Values[Index * 4 + 3]);
    }
}

EAssetResult DecodePrimitive(
    const cgltf_data& Data,
    const FBufferStorage& Storage,
    const cgltf_primitive& SourcePrimitive,
    Core::uint32 MeshIndex,
    Core::uint32 PrimitiveIndex,
    const FStaticModelImportProfile& Profile,
    FStaticMeshPrimitive& OutPrimitive)
{
    OutPrimitive = {};
    if (SourcePrimitive.type != cgltf_primitive_type_triangles ||
        SourcePrimitive.targets_count != 0 ||
        SourcePrimitive.has_draco_mesh_compression)
    {
        return EAssetResult::Unsupported;
    }
    const cgltf_accessor* Position = FindAttribute(
        SourcePrimitive, cgltf_attribute_type_position);
    const cgltf_accessor* Normal = FindAttribute(
        SourcePrimitive, cgltf_attribute_type_normal);
    const cgltf_accessor* Tangent = FindAttribute(
        SourcePrimitive, cgltf_attribute_type_tangent);
    if (Position == nullptr)
    {
        return EAssetResult::MalformedSource;
    }

    FGLTFDecodedAccessor Decoded;
    EAssetResult Result = DecodeAttribute(
        Data, Storage, Position, EGLTFAccessorType::Vec3,
        Profile.Limits.MaxVerticesPerPrimitive, Decoded);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    CopyVec3(Decoded, OutPrimitive.Vertices.Positions);
    const Core::uint32 VertexCount = Decoded.ElementCount;

    if (Normal != nullptr)
    {
        Result = DecodeAttribute(
            Data, Storage, Normal, EGLTFAccessorType::Vec3,
            Profile.Limits.MaxVerticesPerPrimitive, Decoded);
        if (Result != EAssetResult::Success || Decoded.ElementCount != VertexCount)
        {
            return EAssetResult::MalformedSource;
        }
        CopyVec3(Decoded, OutPrimitive.Vertices.Normals);
    }
    else if (Profile.NormalPolicy == EStaticMeshNormalPolicy::RequireSource)
    {
        return EAssetResult::MalformedSource;
    }
    if (Tangent != nullptr && Normal != nullptr)
    {
        Result = DecodeAttribute(
            Data, Storage, Tangent, EGLTFAccessorType::Vec4,
            Profile.Limits.MaxVerticesPerPrimitive, Decoded);
        if (Result != EAssetResult::Success || Decoded.ElementCount != VertexCount)
        {
            return EAssetResult::MalformedSource;
        }
        CopyVec4(Decoded, OutPrimitive.Vertices.Tangents);
    }
    for (Core::uint32 Set = 0; Set < Profile.MaximumTexCoordSets; ++Set)
    {
        if (const cgltf_accessor* TexCoord = FindAttribute(
                SourcePrimitive, cgltf_attribute_type_texcoord,
                static_cast<cgltf_int>(Set)))
        {
            Result = DecodeAttribute(
                Data, Storage, TexCoord, EGLTFAccessorType::Vec2,
                Profile.Limits.MaxVerticesPerPrimitive, Decoded);
            if (Result != EAssetResult::Success || Decoded.ElementCount != VertexCount)
            {
                return EAssetResult::MalformedSource;
            }
            CopyVec2(Decoded, OutPrimitive.Vertices.TexCoords[Set]);
        }
    }

    Core::TArray<Core::uint32> Indices;
    if (SourcePrimitive.indices != nullptr)
    {
        FGLTFAccessorSource IndexSource;
        Result = MakeAccessorSource(
            Data, Storage, *SourcePrimitive.indices, IndexSource);
        if (Result != EAssetResult::Success ||
            IndexSource.Type != EGLTFAccessorType::Scalar ||
            IndexSource.Count > Profile.Limits.MaxIndicesPerPrimitive)
        {
            return EAssetResult::MalformedSource;
        }
        Result = DecodeGLTFAccessorToIndices(
            IndexSource, Profile.Limits.MaxIndicesPerPrimitive, Indices);
        if (Result != EAssetResult::Success)
        {
            return Result;
        }
    }
    else
    {
        if (VertexCount > Profile.Limits.MaxIndicesPerPrimitive)
        {
            return EAssetResult::CapacityExceeded;
        }
        Indices.reserve(VertexCount);
        for (Core::uint32 Index = 0; Index < VertexCount; ++Index)
        {
            Indices.push_back(Index);
        }
    }
    Result = FStaticMeshIndexData::Create(std::move(Indices), OutPrimitive.Indices);
    if (Result != EAssetResult::Success)
    {
        return EAssetResult::MalformedSource;
    }
    Result = FGLTFGeometryNormalizer::NormalizeSourceStreams(
        OutPrimitive.Vertices);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    if (OutPrimitive.Vertices.Normals.empty())
    {
        OutPrimitive.Vertices.Tangents.clear();
        Result = GenerateFlatStaticMeshNormals(
            OutPrimitive.Vertices, OutPrimitive.Indices,
            Profile.Limits.MaxVerticesPerPrimitive);
        if (Result != EAssetResult::Success)
        {
            return Result;
        }
    }
    Result = BuildStaticMeshBounds(
        OutPrimitive.Vertices.Positions, OutPrimitive.LocalBounds);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    OutPrimitive.StableKey = Core::FString(
        "idx.mesh." + std::to_string(MeshIndex) + ".primitive." +
        std::to_string(PrimitiveIndex));
    OutPrimitive.SourcePrimitiveIndex = PrimitiveIndex;
    OutPrimitive.MaterialSlotIndex = 0;
    return EAssetResult::Success;
}

EAssetResult MakeId(
    const char* Type,
    const Core::FString& LogicalPath,
    const std::string& Subresource,
    FAssetId& OutId)
{
    return FAssetId::Create(
        Core::FString(Type), LogicalPath,
        std::optional<Core::FString>(Core::FString(Subresource)), OutId);
}

FAssetDigest MakeContentDigest(
    const FAssetDigest& SourceDigest,
    const FAssetDigest& ProfileDigest,
    Core::uint32 MeshIndex)
{
    Core::TArray<Core::uint8> Bytes;
    Bytes.insert(Bytes.end(), SourceDigest.GetBytes().begin(), SourceDigest.GetBytes().end());
    Bytes.insert(Bytes.end(), ProfileDigest.GetBytes().begin(), ProfileDigest.GetBytes().end());
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(MeshIndex >> Shift));
    }
    return FAssetDigest::FromBytes(Bytes);
}

FAssetMetadata MakeMaterialMetadata(
    const FMaterialAsset& Material,
    const FAssetSourceLocator& Source)
{
    FAssetMetadata Metadata;
    Metadata.Id = Material.GetDesc().Id;
    Metadata.Version = Material.GetDesc().Version;
    Metadata.Source = Source;
    Metadata.Producer = ImporterId();
    Metadata.ProducerVersion = ImporterVersion();
    Metadata.Dependencies = Material.GetDesc().Dependencies;
    Metadata.Attributes = {{Core::FString("material.schema"), Core::FString("2")}};
    return Metadata;
}

EAssetResult ImportMeshes(
    const FAssetImportRequest& Request,
    const FStaticModelImportProfile& Profile,
    std::span<const Core::uint8> SourceBytes,
    const Core::TSharedPtr<IAssetResolver>& Resolver,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    FCgltfDocument Document;
    EAssetResult Result = FCgltfDocument::Parse(
        SourceBytes, Profile, Document, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    const cgltf_data* Data = Document.GetNativeDocument();
    if (Data == nullptr || Data->meshes_count == 0)
        return EAssetResult::MalformedSource;
    if (Data->meshes_count > Profile.Limits.MaxMeshes ||
        Data->scenes_count > Profile.Limits.MaxScenes ||
        Data->nodes_count > Profile.Limits.MaxNodes ||
        Data->materials_count > Profile.Limits.MaxMaterials ||
        Data->textures_count > Profile.Limits.MaxTextures ||
        Data->images_count > Profile.Limits.MaxImages)
        return EAssetResult::CapacityExceeded;
    Result = ValidateGLTFStaticPackageSupport(*Data);
    if (Result != EAssetResult::Success) return Result;
    Core::uint64 AggregateDependencyBytes = 0;
    FBufferStorage Storage;
    Result = BuildBufferStorage(
        *Data, Profile, Request.Descriptor.Location, Resolver,
        AggregateDependencyBytes, Storage);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    const Core::FString LogicalPath = Request.Descriptor.Location.GetLocator();
    FGLTFPackageIdentityPlan Identities;
    Result = PlanGLTFPackageIdentities(
        *Data, LogicalPath, Profile, Identities);
    if (Result != EAssetResult::Success)
    {
        AddDiagnostic(Diagnostics, EAssetStage::Identity, Result, Request,
            "asset.gltf.identity", "package-identities");
        return Result;
    }
    const FAssetDigest SourceDigest = FAssetDigest::FromBytes(SourceBytes);
    const FAssetDigest ProfileDigest = Profile.GetDigest();
    FAssetId SourceId;
    if (MakeId("StaticModelSource", LogicalPath, "source", SourceId) !=
        EAssetResult::Success)
    {
        return EAssetResult::InvalidIdentity;
    }
    FAssetVersion SourceVersion;
    SourceVersion.SourceDigest = SourceDigest;
    SourceVersion.ContentDigest = SourceDigest;
    SourceVersion.Producer = ImporterId();
    SourceVersion.ProducerVersion = ImporterVersion();
    const FAssetSourceVersionRecord SourceRecord{
        SourceId, SourceVersion, EAssetSourceRole::Source};

    FGLTFMaterialMappingProfile Mapping;
    Result = MakeDefaultGLTFMaterialMappingProfile(Mapping);
    if (Result != EAssetResult::Success ||
        Mapping.Name != Profile.MaterialMappingProfile)
        return EAssetResult::Unsupported;
    Core::TArray<FGLTFTextureVariant> TextureVariants;
    Result = PlanGLTFTextureVariants(*Data, Identities, TextureVariants);
    if (Result != EAssetResult::Success)
    {
        AddDiagnostic(Diagnostics, EAssetStage::Identity, Result, Request,
            "asset.gltf.texture-identity", "texture-variants");
        return Result;
    }
    Core::TArray<FAssetImportOutput> ImageTextureOutputs;
    Result = BuildGLTFImageTextureOutputs(
        *Data, Identities, TextureVariants, Request, Resolver, Profile,
        AggregateDependencyBytes,
        [&Data, &Storage](const cgltf_buffer_view* View,
            Core::TArray<Core::uint8>& OutBytes)
        {
            std::span<const Core::uint8> Buffer;
            const EAssetResult ReadResult =
                GetBufferViewBytes(*Data, Storage, View, Buffer);
            if (ReadResult != EAssetResult::Success ||
                View->offset > Buffer.size() || View->size > Buffer.size() - View->offset)
                return EAssetResult::MalformedSource;
            OutBytes.assign(Buffer.begin() + static_cast<std::ptrdiff_t>(View->offset),
                Buffer.begin() + static_cast<std::ptrdiff_t>(View->offset + View->size));
            return EAssetResult::Success;
        },
        ImageTextureOutputs, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        AddDiagnostic(Diagnostics, EAssetStage::Dependency, Result, Request,
            "asset.gltf.image", "image-texture-outputs");
        return Result;
    }
    OutOutputs.insert(OutOutputs.end(),
        ImageTextureOutputs.begin(), ImageTextureOutputs.end());

    Core::TArray<Core::TSharedPtr<const FMaterialAsset>> Materials;
    Materials.reserve(Data->materials_count + 1);
    for (Core::uint32 MaterialIndex = 0;
         MaterialIndex < Data->materials_count; ++MaterialIndex)
    {
        Core::TSharedPtr<const FMaterialAsset> Material;
        Result = MapGLTFMaterial(*Data, &Data->materials[MaterialIndex],
            Identities.MaterialIds[MaterialIndex], TextureVariants,
            Mapping, Material, Diagnostics);
        if (Result != EAssetResult::Success)
        {
            AddDiagnostic(Diagnostics, EAssetStage::Import, Result, Request,
                "asset.gltf.material", "mapped-material");
            return Result;
        }
        OutOutputs.push_back({MakeMaterialMetadata(
            *Material, Request.Descriptor.Location), Material});
        Materials.push_back(std::move(Material));
    }
    Core::TSharedPtr<const FMaterialAsset> DefaultMaterial;
    Result = BuildGLTFDefaultMaterial(*Data, Identities.DefaultMaterialId,
        TextureVariants, Mapping, DefaultMaterial, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        AddDiagnostic(Diagnostics, EAssetStage::Import, Result, Request,
            "asset.gltf.default-material", "default-material");
        return Result;
    }
    OutOutputs.push_back({MakeMaterialMetadata(
        *DefaultMaterial, Request.Descriptor.Location), DefaultMaterial});

    Core::uint64 PrimitiveCount = 0;
    Core::uint64 DecodedGeometryBytes = 0;
    Core::TArray<Core::TSharedPtr<const FStaticMeshAsset>> MeshPayloads;
    MeshPayloads.reserve(Data->meshes_count);
    for (cgltf_size MeshIndex = 0; MeshIndex < Data->meshes_count; ++MeshIndex)
    {
        const cgltf_mesh& SourceMesh = Data->meshes[MeshIndex];
        if (SourceMesh.primitives_count == 0 ||
            SourceMesh.primitives_count > Profile.Limits.MaxPrimitives - PrimitiveCount)
        {
            return EAssetResult::CapacityExceeded;
        }
        PrimitiveCount += SourceMesh.primitives_count;
        FStaticMeshAssetDesc Desc;
        Desc.Id = Identities.MeshIds[MeshIndex];
        Desc.Version.SourceDigest = SourceDigest;
        Desc.Version.ContentDigest = MakeContentDigest(
            SourceDigest, ProfileDigest, static_cast<Core::uint32>(MeshIndex));
        Desc.Version.Producer = ImporterId();
        Desc.Version.ProducerVersion = ImporterVersion();
        Desc.ImportProfileDigest = ProfileDigest;
        Desc.SourceManifest.push_back(SourceRecord);

        for (Core::uint32 MaterialIndex = 0;
             MaterialIndex < Data->materials_count; ++MaterialIndex)
        {
            FStaticMeshMaterialSlot Slot;
            Slot.StableKey = Identities.MaterialKeys[MaterialIndex];
            if (TSoftAssetRef<FMaterialAsset>::Create(
                    Identities.MaterialIds[MaterialIndex], Slot.Material) !=
                EAssetResult::Success) return EAssetResult::InvalidIdentity;
            Desc.MaterialSlots.push_back(std::move(Slot));
            Desc.Dependencies.push_back({Identities.MaterialIds[MaterialIndex],
                EAssetDependencyRole::Runtime, EAssetDependencyStrength::Required,
                EAssetDependencyResolution::Unresolved});
        }
        FStaticMeshMaterialSlot DefaultSlot;
        DefaultSlot.StableKey = Core::FString("idx.material.default");
        if (TSoftAssetRef<FMaterialAsset>::Create(
                Identities.DefaultMaterialId, DefaultSlot.Material) !=
            EAssetResult::Success) return EAssetResult::InvalidIdentity;
        Desc.MaterialSlots.push_back(std::move(DefaultSlot));
        Desc.Dependencies.push_back({
            Identities.DefaultMaterialId,
            EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Unresolved});

        Desc.Primitives.reserve(SourceMesh.primitives_count);
        for (cgltf_size PrimitiveIndex = 0;
             PrimitiveIndex < SourceMesh.primitives_count; ++PrimitiveIndex)
        {
            FStaticMeshPrimitive Primitive;
            Result = DecodePrimitive(
                *Data, Storage, SourceMesh.primitives[PrimitiveIndex],
                static_cast<Core::uint32>(MeshIndex),
                static_cast<Core::uint32>(PrimitiveIndex), Profile, Primitive);
            if (Result != EAssetResult::Success)
            {
                return Result;
            }
            const cgltf_primitive& SourcePrimitive =
                SourceMesh.primitives[PrimitiveIndex];
            Result = ValidateMaterialTexCoords(SourcePrimitive);
            if (Result != EAssetResult::Success) return Result;
            if (SourcePrimitive.material != nullptr)
            {
                if (SourcePrimitive.material < Data->materials ||
                    SourcePrimitive.material >= Data->materials + Data->materials_count)
                    return EAssetResult::MalformedSource;
                Primitive.MaterialSlotIndex = static_cast<Core::uint32>(
                    SourcePrimitive.material - Data->materials);
                if (SourcePrimitive.material->normal_texture.texture != nullptr &&
                    Primitive.Vertices.Tangents.empty())
                {
                    const cgltf_int TexCoord =
                        SourcePrimitive.material->normal_texture.texcoord;
                    if (TexCoord < 0 || TexCoord > 1 ||
                        (Profile.TangentPolicy ==
                            EStaticMeshTangentPolicy::RequireSource))
                        return EAssetResult::MalformedSource;
                    Result = GenerateStaticMeshTangents(
                        Primitive.Vertices, Primitive.Indices,
                        static_cast<Core::uint32>(TexCoord),
                        Profile.Limits.MaxVerticesPerPrimitive);
                    if (Result != EAssetResult::Success) return Result;
                }
            }
            else Primitive.MaterialSlotIndex =
                static_cast<Core::uint32>(Data->materials_count);
            Result = AccountDecodedGeometry(
                Primitive, Profile.Limits.MaxDecodedGeometryBytes,
                DecodedGeometryBytes);
            if (Result != EAssetResult::Success) return Result;
            bool ExplicitPrimitiveKey = false;
            Result = MakeGLTFStableKey(
                SourceMesh.primitives[PrimitiveIndex].extras.data,
                Core::FString(
                    "idx.mesh." + std::to_string(MeshIndex) + ".primitive." +
                    std::to_string(PrimitiveIndex)),
                Primitive.StableKey,
                ExplicitPrimitiveKey);
            (void)ExplicitPrimitiveKey;
            if (Result != EAssetResult::Success)
            {
                return Result;
            }
            Desc.Primitives.push_back(std::move(Primitive));
        }
        Result = BuildAggregateStaticMeshBounds(Desc.Primitives, Desc.Bounds);
        if (Result != EAssetResult::Success)
        {
            return Result;
        }
        FStaticMeshAsset Asset;
        Result = FStaticMeshAsset::CreateValidated(
            std::move(Desc), Asset, Diagnostics);
        if (Result != EAssetResult::Success)
        {
            return Result;
        }
        auto Payload = Core::MakeShared<const FStaticMeshAsset>(std::move(Asset));
        MeshPayloads.push_back(Payload);
        FAssetMetadata Metadata;
        Metadata.Id = Payload->GetDesc().Id;
        Metadata.Version = Payload->GetDesc().Version;
        Metadata.Source = Request.Descriptor.Location;
        Metadata.Producer = ImporterId();
        Metadata.ProducerVersion = ImporterVersion();
        Metadata.Dependencies = Payload->GetDesc().Dependencies;
        Metadata.Attributes = {
            {Core::FString("mesh.primitive-count"),
             Core::FString(std::to_string(Payload->GetDesc().Primitives.size()))},
            {Core::FString("mesh.profile"), ProfileDigest.ToLowerHex()}};
        OutOutputs.push_back({std::move(Metadata), std::move(Payload)});
    }
    Core::TArray<Core::TSharedPtr<const FStaticModelAsset>> Models;
    Result = AssembleGLTFModels(
        *Data, Identities, Profile, SourceRecord,
        ImporterId(), ImporterVersion(), MeshPayloads, Models, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    for (const auto& Model : Models)
    {
        FAssetMetadata Metadata;
        Metadata.Id = Model->GetDesc().Id;
        Metadata.Version = Model->GetDesc().Version;
        Metadata.Source = Request.Descriptor.Location;
        Metadata.Producer = ImporterId();
        Metadata.ProducerVersion = ImporterVersion();
        Metadata.Dependencies = Model->GetDesc().Dependencies;
        Metadata.Attributes = {
            {Core::FString("model.node-count"),
             Core::FString(std::to_string(Model->GetDesc().Nodes.size()))},
            {Core::FString("model.scene-key"), Model->GetDesc().SceneStableKey},
            {Core::FString("model.default-scene"),
             Core::FString(Model->GetDesc().bSourceDefaultScene ? "true" : "false")}};
        Metadata.Attributes.push_back({Core::FString("model.skipped-cameras"),
            Core::FString(std::to_string(Data->cameras_count))});
        Metadata.Attributes.push_back({Core::FString("model.skipped-lights"),
            Core::FString(std::to_string(Data->lights_count))});
        Metadata.Attributes.push_back({Core::FString("model.skipped-animations"),
            Core::FString(std::to_string(Data->animations_count))});
        Metadata.Attributes.push_back({Core::FString("model.optional-extensions"),
            OptionalExtensionSummary(*Data)});
        OutOutputs.push_back({std::move(Metadata), Model});
    }
    std::sort(OutOutputs.begin(), OutOutputs.end(),
        [](const FAssetImportOutput& Left, const FAssetImportOutput& Right)
        {
            return Left.Metadata.Id < Right.Metadata.Id;
        });
    Core::TArray<FAssetId> ExpectedTextureIds;
    ExpectedTextureIds.reserve(TextureVariants.size());
    for (const FGLTFTextureVariant& Variant : TextureVariants)
        ExpectedTextureIds.push_back(Variant.TextureId);
    return ValidateGLTFPackageOutputs(
        Identities, OutOutputs, true, ExpectedTextureIds);
}

} // namespace

FAssetExtensionCapability FGLTFStaticModelImporter::GetCapability() const
{
    FAssetExtensionCapability Capability;
    Capability.Kind = EAssetExtensionKind::Importer;
    Capability.Participant = ImporterId();
    Capability.ProducerVersion = ImporterVersion();
    Capability.FormatHints = {Core::FString("gltf"), Core::FString("glb")};
    Capability.ProbeByteLimit = 64U * 1024U;
    return Capability;
}

FAssetProbeResult FGLTFStaticModelImporter::Probe(
    const FAssetSourceDescriptor& Descriptor,
    std::span<const Core::uint8> Prefix)
{
    (void)Descriptor;
    if (Prefix.size() >= 4 && Prefix[0] == 'g' && Prefix[1] == 'l' &&
        Prefix[2] == 'T' && Prefix[3] == 'F')
    {
        return {EAssetResult::Success, 100, Core::FString("GLB signature")};
    }
    const std::string_view Text(
        reinterpret_cast<const char*>(Prefix.data()), Prefix.size());
    if (Text.find("\"asset\"") != std::string_view::npos &&
        Text.find("\"version\"") != std::string_view::npos &&
        Text.find("\"2.0\"") != std::string_view::npos)
    {
        return {EAssetResult::Success, 90, Core::FString("glTF 2.0 JSON")};
    }
    return {EAssetResult::Success, 0, Core::FString("unrecognized source")};
}

EAssetResult FGLTFStaticModelImporter::Import(
    const FAssetSourceDescriptor& Descriptor,
    const FAssetSourceLease& Source,
    Core::TArray<FAssetImportOutput>& OutOutputs)
{
    return Import(FAssetImportRequest{Descriptor, Source, {}}, OutOutputs, nullptr);
}

EAssetResult FGLTFStaticModelImporter::Import(
    const FAssetImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs)
{
    return Import(Request, OutOutputs, nullptr);
}

EAssetResult FGLTFStaticModelImporter::Import(
    const FAssetImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    if (Diagnostics != nullptr) Diagnostics->clear();
    const auto Profile = std::dynamic_pointer_cast<const FStaticModelImportProfile>(
        Request.Parameters);
    if (!Profile || Profile->Validate() != EAssetResult::Success ||
        !Request.Descriptor.Location.IsValid() || !Request.Source.IsValid())
    {
        AddDiagnostic(Diagnostics, EAssetStage::Validate,
            EAssetResult::InvalidInput, Request, "asset.gltf.request", "request");
        return EAssetResult::InvalidInput;
    }
    Core::TArray<Core::uint8> SourceBytes;
    Core::TArray<FAssetImportOutput> CandidateOutputs;
    EAssetResult Result = Request.Source.ReadBounded(
        Profile->Limits.MaxMainSourceBytes,
        Request.Descriptor.Size,
        SourceBytes);
    if (Result == EAssetResult::Success)
    {
        Result = ImportMeshes(
            Request, *Profile, SourceBytes, nullptr, CandidateOutputs, Diagnostics);
    }
    if (Result != EAssetResult::Success)
    {
        AddDiagnostic(Diagnostics, EAssetStage::Import, Result, Request,
            "asset.gltf.import", "package");
    }
    else OutOutputs = std::move(CandidateOutputs);
    if (Diagnostics != nullptr && Diagnostics->size() > Profile->Limits.MaxDiagnostics)
        Diagnostics->resize(Profile->Limits.MaxDiagnostics);
    return Result;
}

EAssetResult FGLTFStaticModelImporter::Import(
    const FStaticModelImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    if (Diagnostics != nullptr) Diagnostics->clear();
    if (!Request.Profile || Request.Profile->Validate() != EAssetResult::Success ||
        !Request.AssetRequest.Descriptor.Location.IsValid() ||
        !Request.AssetRequest.Source.IsValid())
        return EAssetResult::InvalidInput;
    Core::TArray<Core::uint8> SourceBytes;
    Core::TArray<FAssetImportOutput> CandidateOutputs;
    EAssetResult Result = Request.AssetRequest.Source.ReadBounded(
        Request.Profile->Limits.MaxMainSourceBytes,
        Request.AssetRequest.Descriptor.Size, SourceBytes);
    if (Result == EAssetResult::Success)
        Result = ImportMeshes(
            Request.AssetRequest, *Request.Profile, SourceBytes,
            Request.DependencyResolver, CandidateOutputs, Diagnostics);
    if (Result == EAssetResult::Success)
        OutOutputs = std::move(CandidateOutputs);
    else AddDiagnostic(Diagnostics, EAssetStage::Import, Result,
        Request.AssetRequest, "asset.gltf.import", "package");
    if (Diagnostics != nullptr &&
        Diagnostics->size() > Request.Profile->Limits.MaxDiagnostics)
        Diagnostics->resize(Request.Profile->Limits.MaxDiagnostics);
    return Result;
}

} // namespace Stoner::Asset::Private
