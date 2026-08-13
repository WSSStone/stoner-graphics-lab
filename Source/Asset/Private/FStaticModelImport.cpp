#include "Asset/FStaticModelImport.h"

#include <cctype>
#include <string_view>

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

void AppendUint64(Core::TArray<Core::uint8>& Bytes, Core::uint64 Value)
{
    for (Core::uint32 Shift = 0; Shift < 64; Shift += 8)
    {
        Bytes.push_back(static_cast<Core::uint8>(Value >> Shift));
    }
}

void AppendString(Core::TArray<Core::uint8>& Bytes, const Core::FString& Value)
{
    AppendUint64(Bytes, Value.Len());
    Bytes.insert(Bytes.end(), Value.View().begin(), Value.View().end());
}

bool IsCanonicalToken(std::string_view Value) noexcept
{
    if (Value.empty() ||
        !std::isalpha(static_cast<unsigned char>(Value.front())))
    {
        return false;
    }
    for (const unsigned char Character : Value)
    {
        if (!std::isalnum(Character) && Character != '_' && Character != '-' &&
            Character != '.')
        {
            return false;
        }
    }
    return true;
}

void AppendLimits(
    Core::TArray<Core::uint8>& Bytes,
    const FStaticModelImportLimits& Limits)
{
    AppendUint64(Bytes, Limits.MaxMainSourceBytes);
    AppendUint64(Bytes, Limits.MaxSingleDependencyBytes);
    AppendUint64(Bytes, Limits.MaxAggregateDependencyBytes);
    AppendUint64(Bytes, Limits.MaxParserAllocationBytes);
    AppendUint32(Bytes, Limits.MaxScenes);
    AppendUint32(Bytes, Limits.MaxNodes);
    AppendUint32(Bytes, Limits.MaxHierarchyDepth);
    AppendUint32(Bytes, Limits.MaxMeshes);
    AppendUint32(Bytes, Limits.MaxPrimitives);
    AppendUint32(Bytes, Limits.MaxMaterials);
    AppendUint32(Bytes, Limits.MaxTextures);
    AppendUint32(Bytes, Limits.MaxImages);
    AppendUint32(Bytes, Limits.MaxVerticesPerPrimitive);
    AppendUint32(Bytes, Limits.MaxIndicesPerPrimitive);
    AppendUint64(Bytes, Limits.MaxDecodedGeometryBytes);
    AppendUint32(Bytes, Limits.MaxDiagnostics);
}

} // namespace

bool IsValidStaticMeshNormalPolicy(EStaticMeshNormalPolicy Value) noexcept
{
    return Value == EStaticMeshNormalPolicy::GenerateFlat ||
        Value == EStaticMeshNormalPolicy::RequireSource;
}

bool IsValidStaticMeshTangentPolicy(EStaticMeshTangentPolicy Value) noexcept
{
    return Value == EStaticMeshTangentPolicy::GenerateWhenRequired ||
        Value == EStaticMeshTangentPolicy::RequireSource;
}

EAssetResult FStaticModelImportLimits::Validate() const noexcept
{
    const bool bNonZero =
        MaxMainSourceBytes != 0 && MaxSingleDependencyBytes != 0 &&
        MaxAggregateDependencyBytes != 0 && MaxParserAllocationBytes != 0 &&
        MaxScenes != 0 && MaxNodes != 0 && MaxHierarchyDepth != 0 &&
        MaxMeshes != 0 && MaxPrimitives != 0 && MaxMaterials != 0 &&
        MaxTextures != 0 && MaxImages != 0 && MaxVerticesPerPrimitive != 0 &&
        MaxIndicesPerPrimitive != 0 && MaxDecodedGeometryBytes != 0 &&
        MaxDiagnostics != 0;
    if (!bNonZero || MaxSingleDependencyBytes > MaxAggregateDependencyBytes)
    {
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

EAssetResult FStaticModelImportProfile::Validate() const noexcept
{
    if (SchemaVersion != 1 ||
        !IsCanonicalToken(ProfileName.View()) ||
        !IsValidStaticMeshNormalPolicy(NormalPolicy) ||
        !IsValidStaticMeshTangentPolicy(TangentPolicy) ||
        (MaximumTexCoordSets != 1 && MaximumTexCoordSets != 2) ||
        !IsCanonicalToken(MaterialMappingProfile.View()) ||
        CoordinateConvention != Core::FString(Core::FCoordinateConvention::Name) ||
        Limits.Validate() != EAssetResult::Success)
    {
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

FAssetDigest FStaticModelImportProfile::GetDigest() const
{
    if (Validate() != EAssetResult::Success)
    {
        return {};
    }
    Core::TArray<Core::uint8> Bytes;
    AppendUint32(Bytes, SchemaVersion);
    AppendString(Bytes, ProfileName);
    Bytes.push_back(static_cast<Core::uint8>(NormalPolicy));
    Bytes.push_back(static_cast<Core::uint8>(TangentPolicy));
    AppendUint32(Bytes, MaximumTexCoordSets);
    AppendString(Bytes, MaterialMappingProfile);
    AppendString(Bytes, CoordinateConvention);
    AppendLimits(Bytes, Limits);
    return FAssetDigest::FromBytes(Bytes);
}

} // namespace Stoner::Asset
