#include "Asset/FStaticModelInspection.h"

#include <bit>
#include <sstream>

namespace Stoner::Asset
{
namespace
{

void AppendUint32(Core::TArray<Core::uint8>& Bytes, Core::uint32 Value)
{
    for (Core::uint32 Shift = 0; Shift < 32; Shift += 8)
        Bytes.push_back(static_cast<Core::uint8>(Value >> Shift));
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

} // namespace

FAssetDigest FStaticModelInspection::ComputeHierarchyDigest(
    const FStaticModelAsset& Asset)
{
    Core::TArray<Core::uint8> Bytes;
    const FStaticModelAssetDesc& Desc = Asset.GetDesc();
    AppendString(Bytes, Desc.SceneStableKey);
    Bytes.push_back(Desc.bSourceDefaultScene ? 1 : 0);
    for (const Core::uint32 Root : Desc.RootNodeIndices) AppendUint32(Bytes, Root);
    for (const FStaticModelNode& Node : Desc.Nodes)
    {
        AppendString(Bytes, Node.StableKey);
        AppendString(Bytes, Node.DisplayName);
        AppendUint32(Bytes, Node.SourceNodeIndex);
        const Core::FMatrix4x4 Matrix = Node.LocalTransform.ToMatrix();
        for (int Row = 0; Row < 4; ++Row)
            for (int Column = 0; Column < 4; ++Column)
                AppendFloat(Bytes, Matrix.M[Row][Column]);
        Bytes.push_back(Node.bNegativeDeterminant ? 1 : 0);
        for (const Core::uint32 Child : Node.Children) AppendUint32(Bytes, Child);
        if (Node.Mesh && Node.Mesh->GetId())
            AppendString(Bytes, Node.Mesh->GetId()->ToString());
    }
    return FAssetDigest::FromBytes(Bytes);
}

Core::FString FStaticModelInspection::Format(const FStaticModelAsset& Asset)
{
    const FStaticModelAssetDesc& Desc = Asset.GetDesc();
    std::ostringstream Stream;
    Stream << "StaticModel id=" << Desc.Id.ToString().CStr()
           << " scene=" << Desc.SceneStableKey.CStr()
           << " default=" << (Desc.bSourceDefaultScene ? "yes" : "no")
           << " nodes=" << Desc.Nodes.size()
           << " roots=" << Desc.RootNodeIndices.size()
           << " hierarchy=" << ComputeHierarchyDigest(Asset).ToLowerHex().CStr()
           << " dependencies=" << Desc.Dependencies.size() << '\n';
    for (Core::usize Index = 0; Index < Desc.Nodes.size(); ++Index)
    {
        const FStaticModelNode& Node = Desc.Nodes[Index];
        Stream << "  node=" << Index << " key=" << Node.StableKey.CStr()
               << " source=" << Node.SourceNodeIndex
               << " children=" << Node.Children.size()
               << " mesh=" << (Node.Mesh && Node.Mesh->GetId()
                    ? Node.Mesh->GetId()->ToString().CStr() : "<none>")
               << " negative=" << (Node.bNegativeDeterminant ? "yes" : "no")
               << '\n';
    }
    return Core::FString(Stream.str());
}

Core::FString FStaticModelInspection::FormatPackage(
    const Core::TArray<FAssetImportOutput>& Outputs)
{
    std::ostringstream Stream;
    Stream << "Package outputs=" << Outputs.size() << '\n';
    for (const FAssetImportOutput& Output : Outputs)
    {
        Stream << "  id=" << Output.Metadata.Id.ToString().CStr()
               << " type=" << (Output.Payload
                    ? Output.Payload->GetAssetType().CStr() : "<metadata>")
               << " dependencies=" << Output.Metadata.Dependencies.size() << '\n';
    }
    return Core::FString(Stream.str());
}

} // namespace Stoner::Asset
