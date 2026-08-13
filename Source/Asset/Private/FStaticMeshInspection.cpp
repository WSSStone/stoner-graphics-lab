#include "Asset/FStaticMeshInspection.h"

#include <bit>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace Stoner::Asset
{
namespace
{

void AppendUint32(Core::TArray<Core::uint8>& Bytes, Core::uint32 Value)
{
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(Value >> Shift));
    }
}

void AppendFloat(Core::TArray<Core::uint8>& Bytes, float Value)
{
    AppendUint32(Bytes, std::bit_cast<Core::uint32>(Value));
}

void AppendString(Core::TArray<Core::uint8>& Bytes, const Core::FString& Value)
{
    AppendUint32(Bytes, static_cast<Core::uint32>(Value.Len()));
    Bytes.insert(Bytes.end(), Value.View().begin(), Value.View().end());
}

void AppendVec2(Core::TArray<Core::uint8>& Bytes, const Core::FVector2& Value)
{
    AppendFloat(Bytes, Value.X);
    AppendFloat(Bytes, Value.Y);
}

void AppendVec3(Core::TArray<Core::uint8>& Bytes, const Core::FVector3& Value)
{
    AppendFloat(Bytes, Value.X);
    AppendFloat(Bytes, Value.Y);
    AppendFloat(Bytes, Value.Z);
}

void AppendVec4(Core::TArray<Core::uint8>& Bytes, const Core::FVector4& Value)
{
    AppendFloat(Bytes, Value.X);
    AppendFloat(Bytes, Value.Y);
    AppendFloat(Bytes, Value.Z);
    AppendFloat(Bytes, Value.W);
}

} // namespace

FAssetDigest FStaticMeshInspection::ComputeGeometryDigest(
    const FStaticMeshAsset& Asset)
{
    Core::TArray<Core::uint8> Bytes;
    const FStaticMeshAssetDesc& Desc = Asset.GetDesc();
    AppendUint32(Bytes, Desc.SchemaVersion);
    AppendUint32(Bytes, static_cast<Core::uint32>(Desc.Primitives.size()));
    for (const FStaticMeshPrimitive& Primitive : Desc.Primitives)
    {
        AppendString(Bytes, Primitive.StableKey);
        AppendUint32(Bytes, Primitive.SourcePrimitiveIndex);
        AppendUint32(Bytes, Primitive.MaterialSlotIndex);
        AppendUint32(Bytes, static_cast<Core::uint32>(Primitive.Vertices.Positions.size()));
        for (const Core::FVector3& Value : Primitive.Vertices.Positions)
            AppendVec3(Bytes, Value);
        for (const Core::FVector3& Value : Primitive.Vertices.Normals)
            AppendVec3(Bytes, Value);
        for (const Core::FVector4& Value : Primitive.Vertices.Tangents)
            AppendVec4(Bytes, Value);
        for (const auto& Set : Primitive.Vertices.TexCoords)
        {
            AppendUint32(Bytes, static_cast<Core::uint32>(Set.size()));
            for (const Core::FVector2& Value : Set) AppendVec2(Bytes, Value);
        }
        AppendUint32(Bytes, Primitive.Indices.GetIndexCount());
        for (Core::uint32 Index = 0; Index < Primitive.Indices.GetIndexCount(); ++Index)
            AppendUint32(Bytes, Primitive.Indices.GetIndex(Index));
        AppendVec3(Bytes, Primitive.LocalBounds.Box.Min);
        AppendVec3(Bytes, Primitive.LocalBounds.Box.Max);
        AppendVec3(Bytes, Primitive.LocalBounds.Sphere.Center);
        AppendFloat(Bytes, Primitive.LocalBounds.Sphere.Radius);
    }
    return FAssetDigest::FromBytes(Bytes);
}

Core::FString FStaticMeshInspection::Format(const FStaticMeshAsset& Asset)
{
    const FStaticMeshAssetDesc& Desc = Asset.GetDesc();
    std::ostringstream Stream;
    Stream.imbue(std::locale::classic());
    Stream << std::setprecision(std::numeric_limits<float>::max_digits10)
           << "StaticMesh id=" << Desc.Id.ToString().CStr()
           << " source=" << Desc.Version.SourceDigest.ToLowerHex().CStr()
           << " content=" << Desc.Version.ContentDigest.ToLowerHex().CStr()
           << " profile=" << Desc.ImportProfileDigest.ToLowerHex().CStr()
           << " geometry=" << ComputeGeometryDigest(Asset).ToLowerHex().CStr()
           << " primitives=" << Desc.Primitives.size()
           << " materials=" << Desc.MaterialSlots.size()
           << " sources=" << Desc.SourceManifest.size()
           << " bounds=(" << Desc.Bounds.Box.Min.X << ',' << Desc.Bounds.Box.Min.Y
           << ',' << Desc.Bounds.Box.Min.Z << ")-(" << Desc.Bounds.Box.Max.X
           << ',' << Desc.Bounds.Box.Max.Y << ',' << Desc.Bounds.Box.Max.Z << ")\n";
    for (const FStaticMeshPrimitive& Primitive : Desc.Primitives)
    {
        Stream << "  primitive=" << Primitive.StableKey.CStr()
               << " sourceIndex=" << Primitive.SourcePrimitiveIndex
               << " material=" << Primitive.MaterialSlotIndex
               << " vertices=" << Primitive.Vertices.Positions.size()
               << " normals=" << Primitive.Vertices.Normals.size()
               << " tangents=" << Primitive.Vertices.Tangents.size()
               << " uv0=" << Primitive.Vertices.TexCoords[0].size()
               << " uv1=" << Primitive.Vertices.TexCoords[1].size()
               << " indices=" << Primitive.Indices.GetIndexCount()
               << " indexWidth=" << (Primitive.Indices.Uses16BitIndices() ? 16 : 32)
               << '\n';
    }
    for (const FAssetSourceVersionRecord& Source : Desc.SourceManifest)
    {
        Stream << "  source=" << Source.Id.ToString().CStr()
               << " digest=" << Source.Version.SourceDigest.ToLowerHex().CStr()
               << " role=" << static_cast<unsigned>(Source.Role) << '\n';
    }
    return Core::FString(Stream.str());
}

} // namespace Stoner::Asset
