#include "FStaticModelCookedCodec.h"

#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "FAssetCookedBinary.h"

#include <memory>
#include <new>

namespace Stoner::Asset::Private
{
namespace
{

void WriteOptionalDigest(FCookedBinaryWriter& Writer, const FAssetDigest& Value)
{
    Writer.Bool(Value.IsAvailable());
    if (Value.IsAvailable()) Writer.Digest(Value);
}

bool ReadOptionalDigest(FCookedBinaryReader& Reader, FAssetDigest& Out)
{
    bool Present = false;
    if (!Reader.Bool(Present)) return false;
    Out = {};
    return !Present || Reader.Digest(Out);
}

void WriteVersion(FCookedBinaryWriter& Writer, const FAssetVersion& Value)
{
    WriteOptionalDigest(Writer, Value.SourceDigest);
    WriteOptionalDigest(Writer, Value.ContentDigest);
    WriteOptionalDigest(Writer, Value.CookDigest);
    Writer.Bool(Value.Producer.IsValid());
    if (Value.Producer.IsValid()) Writer.Text(Value.Producer.ToString());
    Writer.Bool(Value.ProducerVersion.IsValid());
    if (Value.ProducerVersion.IsValid())
        Writer.Text(Value.ProducerVersion.ToString());
    Writer.OptionalText(Value.TargetProfile);
}

bool ReadVersion(FCookedBinaryReader& Reader, FAssetVersion& Out)
{
    bool HasProducer = false;
    bool HasProducerVersion = false;
    Core::FString Producer;
    Core::FString ProducerVersion;
    if (!ReadOptionalDigest(Reader, Out.SourceDigest) ||
        !ReadOptionalDigest(Reader, Out.ContentDigest) ||
        !ReadOptionalDigest(Reader, Out.CookDigest) ||
        !Reader.Bool(HasProducer) ||
        (HasProducer && !Reader.Text(Producer)) ||
        !Reader.Bool(HasProducerVersion) ||
        (HasProducerVersion && !Reader.Text(ProducerVersion)) ||
        !Reader.OptionalText(Out.TargetProfile))
        return false;
    if (HasProducer && FAssetParticipantId::Create(Producer, Out.Producer) !=
            EAssetResult::Success)
        return false;
    if (HasProducerVersion && FAssetProducerVersion::Create(
            ProducerVersion, Out.ProducerVersion) != EAssetResult::Success)
        return false;
    return Out.Validate() == EAssetResult::Success;
}

void Vec2(FCookedBinaryWriter& Writer, const Core::FVector2& Value)
{
    Writer.Float(Value.X); Writer.Float(Value.Y);
}

void Vec3(FCookedBinaryWriter& Writer, const Core::FVector3& Value)
{
    Writer.Float(Value.X); Writer.Float(Value.Y); Writer.Float(Value.Z);
}

void Vec4(FCookedBinaryWriter& Writer, const Core::FVector4& Value)
{
    Writer.Float(Value.X); Writer.Float(Value.Y);
    Writer.Float(Value.Z); Writer.Float(Value.W);
}

bool Vec2(FCookedBinaryReader& Reader, Core::FVector2& Out)
{
    return Reader.Float(Out.X) && Reader.Float(Out.Y);
}

bool Vec3(FCookedBinaryReader& Reader, Core::FVector3& Out)
{
    return Reader.Float(Out.X) && Reader.Float(Out.Y) && Reader.Float(Out.Z);
}

bool Vec4(FCookedBinaryReader& Reader, Core::FVector4& Out)
{
    return Reader.Float(Out.X) && Reader.Float(Out.Y) &&
        Reader.Float(Out.Z) && Reader.Float(Out.W);
}

template <typename T, typename F>
void WriteArray(FCookedBinaryWriter& Writer, const Core::TArray<T>& Values, F Write)
{
    Writer.U32(static_cast<Core::uint32>(Values.size()));
    for (const auto& Value : Values) Write(Writer, Value);
}

template <typename T, typename F>
bool ReadArray(FCookedBinaryReader& Reader, Core::TArray<T>& Out, F Read)
{
    Core::uint32 Count = 0;
    if (!Reader.Count(Count)) return false;
    Out.reserve(Count);
    for (Core::uint32 Index = 0; Index < Count; ++Index)
    {
        T Value;
        if (!Read(Reader, Value)) return false;
        Out.push_back(std::move(Value));
    }
    return true;
}

void WriteBounds(FCookedBinaryWriter& Writer, const FStaticMeshBounds& Bounds)
{
    Vec3(Writer, Bounds.Box.Min);
    Vec3(Writer, Bounds.Box.Max);
    Vec3(Writer, Bounds.Sphere.Center);
    Writer.Float(Bounds.Sphere.Radius);
}

bool ReadBounds(FCookedBinaryReader& Reader, FStaticMeshBounds& Out)
{
    Core::FVector3 Min;
    Core::FVector3 Max;
    Core::FVector3 Center;
    float Radius = 0.0f;
    if (!Vec3(Reader, Min) || !Vec3(Reader, Max) ||
        !Vec3(Reader, Center) || !Reader.Float(Radius))
        return false;
    Out.Box = Core::FBox(Min, Max);
    Out.Sphere = Core::FSphere(Center, Radius);
    return Out.IsValid();
}

void WriteDependency(FCookedBinaryWriter& Writer, const FAssetDependency& Value)
{
    Writer.AssetId(Value.TargetId);
    Writer.U8(static_cast<Core::uint8>(Value.Role));
    Writer.U8(static_cast<Core::uint8>(Value.Strength));
    Writer.U8(static_cast<Core::uint8>(Value.Resolution));
}

bool ReadDependency(FCookedBinaryReader& Reader, FAssetDependency& Out)
{
    Core::uint8 Role = 0;
    Core::uint8 Strength = 0;
    Core::uint8 Resolution = 0;
    if (!Reader.AssetId(Out.TargetId) || !Reader.U8(Role) ||
        !Reader.U8(Strength) || !Reader.U8(Resolution))
        return false;
    Out.Role = static_cast<EAssetDependencyRole>(Role);
    Out.Strength = static_cast<EAssetDependencyStrength>(Strength);
    Out.Resolution = static_cast<EAssetDependencyResolution>(Resolution);
    return true;
}

void WriteSource(FCookedBinaryWriter& Writer, const FAssetSourceVersionRecord& Value)
{
    Writer.AssetId(Value.Id);
    WriteVersion(Writer, Value.Version);
    Writer.U8(static_cast<Core::uint8>(Value.Role));
}

bool ReadSource(FCookedBinaryReader& Reader, FAssetSourceVersionRecord& Out)
{
    Core::uint8 Role = 0;
    if (!Reader.AssetId(Out.Id) || !ReadVersion(Reader, Out.Version) ||
        !Reader.U8(Role))
        return false;
    Out.Role = static_cast<EAssetSourceRole>(Role);
    return true;
}

void WriteMesh(FCookedBinaryWriter& Writer, const FStaticMeshAssetDesc& Desc)
{
    Writer.AssetId(Desc.Id);
    WriteVersion(Writer, Desc.Version);
    Writer.U32(Desc.SchemaVersion);
    Writer.U32(static_cast<Core::uint32>(Desc.Primitives.size()));
    for (const auto& Primitive : Desc.Primitives)
    {
        Writer.Text(Primitive.StableKey);
        WriteArray(Writer, Primitive.Vertices.Positions,
            [](auto& W, const auto& V) { Vec3(W, V); });
        WriteArray(Writer, Primitive.Vertices.Normals,
            [](auto& W, const auto& V) { Vec3(W, V); });
        WriteArray(Writer, Primitive.Vertices.Tangents,
            [](auto& W, const auto& V) { Vec4(W, V); });
        WriteArray(Writer, Primitive.Vertices.TexCoords[0],
            [](auto& W, const auto& V) { Vec2(W, V); });
        WriteArray(Writer, Primitive.Vertices.TexCoords[1],
            [](auto& W, const auto& V) { Vec2(W, V); });
        Writer.U32(Primitive.Indices.GetIndexCount());
        for (Core::uint32 Index = 0; Index < Primitive.Indices.GetIndexCount(); ++Index)
            Writer.U32(Primitive.Indices.GetIndex(Index));
        Writer.U32(Primitive.MaterialSlotIndex);
        WriteBounds(Writer, Primitive.LocalBounds);
        Writer.U32(Primitive.SourcePrimitiveIndex);
    }
    Writer.U32(static_cast<Core::uint32>(Desc.MaterialSlots.size()));
    for (const auto& Slot : Desc.MaterialSlots)
    {
        Writer.Text(Slot.StableKey);
        Writer.Bool(Slot.Material.GetId().has_value());
        if (Slot.Material.GetId()) Writer.AssetId(*Slot.Material.GetId());
    }
    WriteBounds(Writer, Desc.Bounds);
    WriteArray(Writer, Desc.Dependencies, WriteDependency);
    WriteArray(Writer, Desc.SourceManifest, WriteSource);
    Writer.Digest(Desc.ImportProfileDigest);
}

bool ReadMesh(FCookedBinaryReader& Reader, FStaticMeshAssetDesc& Out)
{
    Core::uint32 PrimitiveCount = 0;
    if (!Reader.AssetId(Out.Id) || !ReadVersion(Reader, Out.Version) ||
        !Reader.U32(Out.SchemaVersion) || !Reader.Count(PrimitiveCount))
        return false;
    Out.Primitives.reserve(PrimitiveCount);
    for (Core::uint32 PrimitiveIndex = 0;
         PrimitiveIndex < PrimitiveCount; ++PrimitiveIndex)
    {
        FStaticMeshPrimitive Primitive;
        Core::TArray<Core::uint32> Indices;
        Core::uint32 IndexCount = 0;
        if (!Reader.Text(Primitive.StableKey) ||
            !ReadArray(Reader, Primitive.Vertices.Positions,
                [](auto& R, auto& V) { return Vec3(R, V); }) ||
            !ReadArray(Reader, Primitive.Vertices.Normals,
                [](auto& R, auto& V) { return Vec3(R, V); }) ||
            !ReadArray(Reader, Primitive.Vertices.Tangents,
                [](auto& R, auto& V) { return Vec4(R, V); }) ||
            !ReadArray(Reader, Primitive.Vertices.TexCoords[0],
                [](auto& R, auto& V) { return Vec2(R, V); }) ||
            !ReadArray(Reader, Primitive.Vertices.TexCoords[1],
                [](auto& R, auto& V) { return Vec2(R, V); }) ||
            !Reader.Count(IndexCount))
            return false;
        Indices.reserve(IndexCount);
        for (Core::uint32 Index = 0; Index < IndexCount; ++Index)
        {
            Core::uint32 Value = 0;
            if (!Reader.U32(Value)) return false;
            Indices.push_back(Value);
        }
        if (FStaticMeshIndexData::Create(
                std::move(Indices), Primitive.Indices) != EAssetResult::Success ||
            !Reader.U32(Primitive.MaterialSlotIndex) ||
            !ReadBounds(Reader, Primitive.LocalBounds) ||
            !Reader.U32(Primitive.SourcePrimitiveIndex))
            return false;
        Out.Primitives.push_back(std::move(Primitive));
    }
    Core::uint32 SlotCount = 0;
    if (!Reader.Count(SlotCount)) return false;
    Out.MaterialSlots.reserve(SlotCount);
    for (Core::uint32 Index = 0; Index < SlotCount; ++Index)
    {
        FStaticMeshMaterialSlot Slot;
        bool HasMaterial = false;
        if (!Reader.Text(Slot.StableKey) || !Reader.Bool(HasMaterial))
            return false;
        if (HasMaterial)
        {
            FAssetId MaterialId;
            if (!Reader.AssetId(MaterialId) ||
                TSoftAssetRef<FMaterialAsset>::Create(
                    MaterialId, Slot.Material) != EAssetResult::Success)
                return false;
        }
        Out.MaterialSlots.push_back(std::move(Slot));
    }
    return ReadBounds(Reader, Out.Bounds) &&
        ReadArray(Reader, Out.Dependencies, ReadDependency) &&
        ReadArray(Reader, Out.SourceManifest, ReadSource) &&
        Reader.Digest(Out.ImportProfileDigest);
}

void WriteModel(FCookedBinaryWriter& Writer, const FStaticModelAssetDesc& Desc)
{
    Writer.AssetId(Desc.Id);
    WriteVersion(Writer, Desc.Version);
    Writer.U32(Desc.SchemaVersion);
    Writer.Text(Desc.SceneStableKey);
    Writer.Bool(Desc.bSourceDefaultScene);
    Writer.U32(static_cast<Core::uint32>(Desc.Nodes.size()));
    for (const auto& Node : Desc.Nodes)
    {
        Writer.Text(Node.StableKey);
        Writer.TextAllowEmpty(Node.DisplayName);
        Vec3(Writer, Node.LocalTransform.Translation);
        Writer.Float(Node.LocalTransform.Rotation.X);
        Writer.Float(Node.LocalTransform.Rotation.Y);
        Writer.Float(Node.LocalTransform.Rotation.Z);
        Writer.Float(Node.LocalTransform.Rotation.W);
        Vec3(Writer, Node.LocalTransform.Scale);
        Writer.U32(static_cast<Core::uint32>(Node.Children.size()));
        for (Core::uint32 Child : Node.Children) Writer.U32(Child);
        Writer.Bool(Node.Mesh.has_value());
        if (Node.Mesh)
        {
            Writer.Bool(Node.Mesh->GetId().has_value());
            if (Node.Mesh->GetId()) Writer.AssetId(*Node.Mesh->GetId());
        }
        Writer.U32(Node.SourceNodeIndex);
        Writer.Bool(Node.bNegativeDeterminant);
    }
    Writer.U32(static_cast<Core::uint32>(Desc.RootNodeIndices.size()));
    for (Core::uint32 Root : Desc.RootNodeIndices) Writer.U32(Root);
    WriteBounds(Writer, Desc.Bounds);
    WriteArray(Writer, Desc.Dependencies, WriteDependency);
    WriteArray(Writer, Desc.SourceManifest, WriteSource);
    Writer.Digest(Desc.ImportProfileDigest);
}

bool ReadModel(FCookedBinaryReader& Reader, FStaticModelAssetDesc& Out)
{
    Core::uint32 NodeCount = 0;
    if (!Reader.AssetId(Out.Id) || !ReadVersion(Reader, Out.Version) ||
        !Reader.U32(Out.SchemaVersion) || !Reader.Text(Out.SceneStableKey) ||
        !Reader.Bool(Out.bSourceDefaultScene) || !Reader.Count(NodeCount))
        return false;
    Out.Nodes.reserve(NodeCount);
    for (Core::uint32 Index = 0; Index < NodeCount; ++Index)
    {
        FStaticModelNode Node;
        Core::uint32 ChildCount = 0;
        if (!Reader.Text(Node.StableKey) ||
            !Reader.TextAllowEmpty(Node.DisplayName) ||
            !Vec3(Reader, Node.LocalTransform.Translation) ||
            !Reader.Float(Node.LocalTransform.Rotation.X) ||
            !Reader.Float(Node.LocalTransform.Rotation.Y) ||
            !Reader.Float(Node.LocalTransform.Rotation.Z) ||
            !Reader.Float(Node.LocalTransform.Rotation.W) ||
            !Vec3(Reader, Node.LocalTransform.Scale) ||
            !Reader.Count(ChildCount))
            return false;
        Node.Children.reserve(ChildCount);
        for (Core::uint32 Child = 0; Child < ChildCount; ++Child)
        {
            Core::uint32 ChildIndex = 0;
            if (!Reader.U32(ChildIndex)) return false;
            Node.Children.push_back(ChildIndex);
        }
        bool HasMesh = false;
        if (!Reader.Bool(HasMesh)) return false;
        if (HasMesh)
        {
            bool HasId = false;
            if (!Reader.Bool(HasId) || !HasId) return false;
            FAssetId MeshId;
            TSoftAssetRef<FStaticMeshAsset> Mesh;
            if (!Reader.AssetId(MeshId) ||
                TSoftAssetRef<FStaticMeshAsset>::Create(
                    MeshId, Mesh) != EAssetResult::Success)
                return false;
            Node.Mesh = std::move(Mesh);
        }
        if (!Reader.U32(Node.SourceNodeIndex) ||
            !Reader.Bool(Node.bNegativeDeterminant))
            return false;
        Out.Nodes.push_back(std::move(Node));
    }
    Core::uint32 RootCount = 0;
    if (!Reader.Count(RootCount)) return false;
    Out.RootNodeIndices.reserve(RootCount);
    for (Core::uint32 Index = 0; Index < RootCount; ++Index)
    {
        Core::uint32 Root = 0;
        if (!Reader.U32(Root)) return false;
        Out.RootNodeIndices.push_back(Root);
    }
    return ReadBounds(Reader, Out.Bounds) &&
        ReadArray(Reader, Out.Dependencies, ReadDependency) &&
        ReadArray(Reader, Out.SourceManifest, ReadSource) &&
        Reader.Digest(Out.ImportProfileDigest);
}

FAssetCookedPayloadHeader Header(const FAssetId& Id, const char* Codec)
{
    FAssetCookedPayloadHeader Value;
    Value.AssetId = Id;
    Value.AssetType = Id.GetAssetType();
    Value.CodecId = Core::FString(Codec);
    Value.CodecVersion = 1;
    Value.PayloadSchemaVersion = 1;
    return Value;
}

} // namespace

EAssetResult EncodeStaticModelCookedBody(
    const FAssetPayload& Payload,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadHeader& OutHeader,
    Core::TArray<Core::uint8>& OutBody)
{
    OutHeader = {};
    OutBody.clear();
    if (Limits.Validate() != EAssetResult::Success)
        return EAssetResult::InvalidInput;
    try
    {
        FCookedBinaryWriter Writer(Limits.MaxBodyBytes);
        if (const auto* Mesh = dynamic_cast<const FStaticMeshAsset*>(&Payload))
        {
            WriteMesh(Writer, Mesh->GetDesc());
            OutHeader = Header(Mesh->GetDesc().Id, "stoner.static-mesh");
        }
        else if (const auto* Model =
                     dynamic_cast<const FStaticModelAsset*>(&Payload))
        {
            WriteModel(Writer, Model->GetDesc());
            OutHeader = Header(Model->GetDesc().Id, "stoner.static-model");
        }
        else return EAssetResult::TypeMismatch;
        OutBody = Writer.Take();
        if (OutBody.empty())
        {
            OutHeader = {};
            return EAssetResult::CapacityExceeded;
        }
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutHeader = {};
        OutBody.clear();
        return EAssetResult::CapacityExceeded;
    }
}

EAssetResult DecodeStaticModelCookedBody(
    const FAssetCookedPayloadHeader& HeaderValue,
    std::span<const Core::uint8> Body,
    Core::TSharedPtr<const FAssetPayload>& OutPayload)
{
    OutPayload.reset();
    try
    {
        FCookedBinaryReader Reader(Body);
        if (HeaderValue.CodecId == Core::FString("stoner.static-mesh"))
        {
            FStaticMeshAssetDesc Desc;
            if (!ReadMesh(Reader, Desc) || !Reader.AtEnd() ||
                Desc.Id != HeaderValue.AssetId)
                return EAssetResult::CorruptPayload;
            FStaticMeshAsset Asset;
            const EAssetResult Result = FStaticMeshAsset::CreateValidated(
                std::move(Desc), Asset, nullptr);
            if (Result != EAssetResult::Success) return Result;
            OutPayload = Core::MakeShared<FStaticMeshAsset>(std::move(Asset));
            return EAssetResult::Success;
        }
        if (HeaderValue.CodecId == Core::FString("stoner.static-model"))
        {
            FStaticModelAssetDesc Desc;
            if (!ReadModel(Reader, Desc) || !Reader.AtEnd() ||
                Desc.Id != HeaderValue.AssetId)
                return EAssetResult::CorruptPayload;
            FStaticModelAsset Asset;
            const EAssetResult Result = FStaticModelAsset::CreateValidated(
                std::move(Desc), 256, Asset, nullptr);
            if (Result != EAssetResult::Success) return Result;
            OutPayload = Core::MakeShared<FStaticModelAsset>(std::move(Asset));
            return EAssetResult::Success;
        }
        return EAssetResult::Unsupported;
    }
    catch (const std::bad_alloc&)
    {
        OutPayload.reset();
        return EAssetResult::CapacityExceeded;
    }
}

} // namespace Stoner::Asset::Private
