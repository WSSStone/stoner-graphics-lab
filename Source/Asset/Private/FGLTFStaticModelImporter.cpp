#include "FGLTFStaticModelImporter.h"

#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelImport.h"
#include "FCgltfDocument.h"
#include "FGLTFAccessorDecoder.h"
#include "FGLTFGeometryNormalizer.h"
#include "FGLTFPackageAssembler.h"
#include "FGLTFPackageIdentityPlanner.h"
#include "FGLTFPackageValidator.h"
#include "FGLTFStableKey.h"
#include "FStaticMeshBounds.h"
#include "FStaticMeshNormalGenerator.h"

#include "cgltf/cgltf.h"

#include <algorithm>
#include <array>
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

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetStage Stage,
    EAssetResult Result,
    const FAssetImportRequest& Request,
    const char* Code,
    const char* Field)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = Stage;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = ImporterId().ToString();
    Diagnostic.Subject = Request.Descriptor.Location.ToString();
    Diagnostic.Field = Core::FString(Field);
    Diagnostics->push_back(std::move(Diagnostic));
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

int DecodeBase64Value(char Character)
{
    if (Character >= 'A' && Character <= 'Z') return Character - 'A';
    if (Character >= 'a' && Character <= 'z') return Character - 'a' + 26;
    if (Character >= '0' && Character <= '9') return Character - '0' + 52;
    if (Character == '+') return 62;
    if (Character == '/') return 63;
    return -1;
}

EAssetResult DecodeDataUri(
    std::string_view Uri,
    Core::uint64 MaximumBytes,
    Core::TArray<Core::uint8>& OutBytes)
{
    OutBytes.clear();
    const std::size_t Comma = Uri.find(',');
    if (!Uri.starts_with("data:") || Comma == std::string_view::npos ||
        !Uri.substr(0, Comma).ends_with(";base64"))
    {
        return EAssetResult::AccessDenied;
    }
    const std::string_view Encoded = Uri.substr(Comma + 1);
    if (Encoded.empty() || Encoded.size() % 4 != 0 ||
        Encoded.size() / 4 * 3 > MaximumBytes)
    {
        return EAssetResult::CapacityExceeded;
    }
    OutBytes.reserve(Encoded.size() / 4 * 3);
    for (std::size_t Offset = 0; Offset < Encoded.size(); Offset += 4)
    {
        std::array<int, 4> Values{};
        int Padding = 0;
        for (std::size_t Index = 0; Index < 4; ++Index)
        {
            const char Character = Encoded[Offset + Index];
            if (Character == '=')
            {
                Values[Index] = 0;
                ++Padding;
            }
            else
            {
                if (Padding != 0 || (Values[Index] = DecodeBase64Value(Character)) < 0)
                {
                    OutBytes.clear();
                    return EAssetResult::MalformedSource;
                }
            }
        }
        if (Padding > 2 || (Padding != 0 && Offset + 4 != Encoded.size()))
        {
            OutBytes.clear();
            return EAssetResult::MalformedSource;
        }
        const Core::uint32 Value =
            (static_cast<Core::uint32>(Values[0]) << 18U) |
            (static_cast<Core::uint32>(Values[1]) << 12U) |
            (static_cast<Core::uint32>(Values[2]) << 6U) |
            static_cast<Core::uint32>(Values[3]);
        OutBytes.push_back(static_cast<Core::uint8>(Value >> 16U));
        if (Padding < 2) OutBytes.push_back(static_cast<Core::uint8>(Value >> 8U));
        if (Padding < 1) OutBytes.push_back(static_cast<Core::uint8>(Value));
    }
    return OutBytes.size() <= MaximumBytes
        ? EAssetResult::Success : EAssetResult::CapacityExceeded;
}

struct FBufferStorage
{
    Core::TArray<Core::TArray<Core::uint8>> Owned;
    Core::TArray<std::span<const Core::uint8>> Views;
};

EAssetResult BuildBufferStorage(
    const cgltf_data& Data,
    const FStaticModelImportProfile& Profile,
    FBufferStorage& OutStorage)
{
    OutStorage = {};
    OutStorage.Owned.resize(Data.buffers_count);
    OutStorage.Views.resize(Data.buffers_count);
    Core::uint64 AggregateBytes = 0;
    bool UsedBinaryChunk = false;
    for (cgltf_size Index = 0; Index < Data.buffers_count; ++Index)
    {
        const cgltf_buffer& Buffer = Data.buffers[Index];
        if (Buffer.size > Profile.Limits.MaxSingleDependencyBytes ||
            Buffer.size > Profile.Limits.MaxAggregateDependencyBytes - AggregateBytes)
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
                return EAssetResult::NotFound;
            }
            const EAssetResult Result = DecodeDataUri(
                Uri, Profile.Limits.MaxSingleDependencyBytes,
                OutStorage.Owned[Index]);
            if (Result != EAssetResult::Success ||
                OutStorage.Owned[Index].size() < Buffer.size)
            {
                return Result == EAssetResult::Success
                    ? EAssetResult::MalformedSource : Result;
            }
            OutStorage.Views[Index] = OutStorage.Owned[Index];
        }
        AggregateBytes += Buffer.size;
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
    OutBytes = Buffer;
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
        if (Result != EAssetResult::Success ||
            Accessor.buffer_view->offset >
                std::numeric_limits<Core::uint64>::max() - Accessor.offset)
        {
            return EAssetResult::MalformedSource;
        }
        OutSource.ByteOffset = Accessor.buffer_view->offset + Accessor.offset;
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
        Sparse.IndicesByteOffset =
            Accessor.sparse.indices_buffer_view->offset +
            Accessor.sparse.indices_byte_offset;
        Sparse.ValuesByteOffset =
            Accessor.sparse.values_buffer_view->offset +
            Accessor.sparse.values_byte_offset;
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

EAssetResult ImportMeshes(
    const FAssetImportRequest& Request,
    const FStaticModelImportProfile& Profile,
    std::span<const Core::uint8> SourceBytes,
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
    if (Data == nullptr || Data->meshes_count == 0 ||
        Data->meshes_count > Profile.Limits.MaxMeshes ||
        Data->scenes_count > Profile.Limits.MaxScenes ||
        Data->nodes_count > Profile.Limits.MaxNodes ||
        Data->materials_count > Profile.Limits.MaxMaterials ||
        Data->textures_count > Profile.Limits.MaxTextures ||
        Data->images_count > Profile.Limits.MaxImages ||
        Data->extensions_required_count != 0)
    {
        return EAssetResult::Unsupported;
    }
    FBufferStorage Storage;
    Result = BuildBufferStorage(*Data, Profile, Storage);
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

    Core::uint64 PrimitiveCount = 0;
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

        FStaticMeshMaterialSlot Slot;
        Slot.StableKey = Core::FString("idx.material.default");
        if (TSoftAssetRef<FMaterialAsset>::Create(
                Identities.DefaultMaterialId, Slot.Material) !=
            EAssetResult::Success)
        {
            return EAssetResult::InvalidIdentity;
        }
        Desc.MaterialSlots.push_back(std::move(Slot));
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
        OutOutputs.push_back({std::move(Metadata), Model});
    }
    std::sort(OutOutputs.begin(), OutOutputs.end(),
        [](const FAssetImportOutput& Left, const FAssetImportOutput& Right)
        {
            return Left.Metadata.Id < Right.Metadata.Id;
        });
    return ValidateGLTFPackageOutputs(Identities, OutOutputs);
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
    OutOutputs.clear();
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
    EAssetResult Result = Request.Source.ReadBounded(
        Profile->Limits.MaxMainSourceBytes,
        Request.Descriptor.Size,
        SourceBytes);
    if (Result == EAssetResult::Success)
    {
        Result = ImportMeshes(
            Request, *Profile, SourceBytes, OutOutputs, Diagnostics);
    }
    if (Result != EAssetResult::Success)
    {
        OutOutputs.clear();
        AddDiagnostic(Diagnostics, EAssetStage::Import, Result, Request,
            "asset.gltf.import", "package");
    }
    return Result;
}

} // namespace Stoner::Asset::Private
