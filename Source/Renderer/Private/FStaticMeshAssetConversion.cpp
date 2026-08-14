#include "Renderer/FStaticMeshAssetConversion.h"

#include "Asset/FStaticMeshAsset.h"
#include "RHI/ERHIResult.h"

#include <cstring>
#include <limits>
#include <sstream>

namespace Stoner::Renderer
{
namespace
{

constexpr Stoner::Core::uint32 VertexStride = 56;
static_assert(sizeof(Stoner::Core::FVector2) == 8);
static_assert(sizeof(Stoner::Core::FVector3) == 12);
static_assert(sizeof(Stoner::Core::FVector4) == 16);

template <typename TValue>
void AppendValue(
    Stoner::Core::TArray<Stoner::Core::uint8>& Bytes,
    const TValue& Value)
{
    const auto* Begin = reinterpret_cast<const Stoner::Core::uint8*>(&Value);
    Bytes.insert(Bytes.end(), Begin, Begin + sizeof(TValue));
}

void SetReason(Stoner::Core::FString* OutReason, const char* Reason)
{
    if (OutReason)
    {
        *OutReason = Stoner::Core::FString(Reason);
    }
}

} // namespace

bool FStaticMeshRealizationProfile::IsValid() const noexcept
{
    return Version == 1 &&
        (IndexPacking == EStaticMeshIndexPackingPolicy::Smallest ||
         IndexPacking == EStaticMeshIndexPackingPolicy::UInt32) &&
        !CoordinateConvention.IsEmpty() &&
        CoordinateConvention ==
            Stoner::Core::FString("LH-XForward-YRight-ZUp-Clockwise");
}

Stoner::Asset::FAssetDigest
FStaticMeshRealizationProfile::ComputeDigest() const
{
    std::ostringstream Stream;
    Stream << "static-mesh-realization:" << Version << ':'
           << static_cast<int>(IndexPacking) << ':'
           << CoordinateConvention.CStr() << ":interleaved-p3n3t4uv2uv2";
    const std::string Text = Stream.str();
    return Stoner::Asset::FAssetDigest::FromBytes(
        std::span<const Stoner::Core::uint8>(
            reinterpret_cast<const Stoner::Core::uint8*>(Text.data()),
            Text.size()));
}

Stoner::RHI::ERHIResult BuildStaticMeshPackingPlan(
    const Stoner::Asset::FStaticMeshAsset& Asset,
    const FStaticMeshRealizationProfile& Profile,
    FStaticMeshPackingPlan& OutPlan,
    Stoner::Core::FString* OutReason)
{
    using namespace Stoner::Core;
    using namespace Stoner::RHI;
    OutPlan = {};
    if (OutReason)
    {
        OutReason->Clear();
    }
    if (!Profile.IsValid())
    {
        SetReason(OutReason, "realization profile is unsupported");
        return ERHIResult::Unsupported;
    }

    const auto& Desc = Asset.GetDesc();
    uint64 TotalVertices = 0;
    uint64 TotalIndices = 0;
    bool bNeedsUInt32 =
        Profile.IndexPacking == EStaticMeshIndexPackingPolicy::UInt32;
    for (const Stoner::Asset::FStaticMeshPrimitive& Primitive : Desc.Primitives)
    {
        const uint64 VertexCount = Primitive.Vertices.Positions.size();
        if (VertexCount > static_cast<uint64>(std::numeric_limits<int32>::max()) ||
            TotalVertices > static_cast<uint64>(std::numeric_limits<int32>::max()) - VertexCount ||
            TotalIndices > std::numeric_limits<uint32>::max() -
                Primitive.Indices.GetIndexCount())
        {
            SetReason(OutReason, "packed mesh offsets exceed RHI limits");
            return ERHIResult::Unavailable;
        }
        TotalVertices += VertexCount;
        TotalIndices += Primitive.Indices.GetIndexCount();
        bNeedsUInt32 = bNeedsUInt32 || !Primitive.Indices.Uses16BitIndices();
    }
    if (TotalVertices == 0 || TotalIndices == 0 ||
        TotalVertices > std::numeric_limits<usize>::max() / VertexStride)
    {
        SetReason(OutReason, "packed mesh byte size is invalid");
        return ERHIResult::Unavailable;
    }

    OutPlan.VertexInput.Stride = VertexStride;
    OutPlan.VertexInput.Attributes = {
        {0, ERHIFormat::R32G32B32_Float, 0},
        {1, ERHIFormat::R32G32B32_Float, 12},
        {2, ERHIFormat::R32G32B32A32_Float, 24},
        {3, ERHIFormat::R32G32_Float, 40},
        {4, ERHIFormat::R32G32_Float, 48}};
    OutPlan.IndexType = bNeedsUInt32
        ? ERHIIndexType::UInt32
        : ERHIIndexType::UInt16;
    const uint64 IndexSize = GetRHIIndexTypeSize(OutPlan.IndexType);
    if (TotalIndices > std::numeric_limits<usize>::max() / IndexSize)
    {
        SetReason(OutReason, "packed index byte size is invalid");
        return ERHIResult::Unavailable;
    }

    try
    {
        OutPlan.VertexBytes.reserve(static_cast<usize>(TotalVertices * VertexStride));
        OutPlan.IndexBytes.reserve(static_cast<usize>(TotalIndices * IndexSize));
        OutPlan.Sections.reserve(Desc.Primitives.size());
        uint32 FirstIndex = 0;
        int32 VertexOffset = 0;
        for (const Stoner::Asset::FStaticMeshPrimitive& Primitive : Desc.Primitives)
        {
            const auto& Vertices = Primitive.Vertices;
            for (usize Index = 0; Index < Vertices.Positions.size(); ++Index)
            {
                const FVector3 Normal = Vertices.Normals.empty()
                    ? FVector3::UnitZ() : Vertices.Normals[Index];
                const FVector4 Tangent = Vertices.Tangents.empty()
                    ? FVector4(1.0f, 0.0f, 0.0f, 1.0f)
                    : Vertices.Tangents[Index];
                const FVector2 UV0 = Vertices.TexCoords[0].empty()
                    ? FVector2::Zero() : Vertices.TexCoords[0][Index];
                const FVector2 UV1 = Vertices.TexCoords[1].empty()
                    ? FVector2::Zero() : Vertices.TexCoords[1][Index];
                AppendValue(OutPlan.VertexBytes, Vertices.Positions[Index]);
                AppendValue(OutPlan.VertexBytes, Normal);
                AppendValue(OutPlan.VertexBytes, Tangent);
                AppendValue(OutPlan.VertexBytes, UV0);
                AppendValue(OutPlan.VertexBytes, UV1);
            }
            for (uint32 Index = 0; Index < Primitive.Indices.GetIndexCount(); ++Index)
            {
                const uint32 Value = Primitive.Indices.GetIndex(Index);
                if (OutPlan.IndexType == ERHIIndexType::UInt16)
                {
                    const uint16 Narrow = static_cast<uint16>(Value);
                    AppendValue(OutPlan.IndexBytes, Narrow);
                }
                else
                {
                    AppendValue(OutPlan.IndexBytes, Value);
                }
            }
            const auto& Slot = Desc.MaterialSlots[Primitive.MaterialSlotIndex];
            OutPlan.Sections.push_back({
                FirstIndex,
                Primitive.Indices.GetIndexCount(),
                VertexOffset,
                *Slot.Material.GetId(),
                Primitive.LocalBounds,
                Primitive.StableKey});
            FirstIndex += Primitive.Indices.GetIndexCount();
            VertexOffset += static_cast<int32>(Vertices.Positions.size());
        }
    }
    catch (const std::bad_alloc&)
    {
        OutPlan = {};
        SetReason(OutReason, "packing allocation failed");
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        OutPlan = {};
        SetReason(OutReason, "packing allocation exceeds container limits");
        return ERHIResult::Unavailable;
    }
    OutPlan.ProfileDigest = Profile.ComputeDigest();
    return ERHIResult::Success;
}

} // namespace Stoner::Renderer
