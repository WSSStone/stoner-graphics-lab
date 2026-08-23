#include "ProductionAssetEquivalence.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <span>
#include <string>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

bool Fail(FProductionAssetEquivalenceReport& Out, const char* Reason)
{
    if (Out.FirstFailure.IsEmpty())
        Out.FirstFailure = Core::FString(Reason);
    return false;
}

bool EqualDependencies(
    const Core::TArray<FAssetDependency>& Left,
    const Core::TArray<FAssetDependency>& Right)
{
    if (Left.size() != Right.size()) return false;
    for (Core::usize Index = 0; Index < Left.size(); ++Index)
    {
        const auto& A = Left[Index];
        const auto& B = Right[Index];
        if (A.TargetId != B.TargetId || A.Role != B.Role ||
            A.Strength != B.Strength || A.Resolution != B.Resolution)
            return false;
    }
    return true;
}

bool EqualShaderBinding(
    const FShaderInterfaceBinding& A,
    const FShaderInterfaceBinding& B)
{
    return A.SetIndex == B.SetIndex &&
        A.BindingIndex == B.BindingIndex && A.Kind == B.Kind &&
        A.ArrayCount == B.ArrayCount && A.Visibility == B.Visibility &&
        A.Name == B.Name;
}

bool EqualShaderRange(
    const FShaderConstantRange& A,
    const FShaderConstantRange& B)
{
    return A.OffsetBytes == B.OffsetBytes &&
        A.SizeBytes == B.SizeBytes && A.Visibility == B.Visibility;
}

const char* ShaderProgramMismatch(
    const FShaderAsset& Left,
    const FShaderAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    if (A.Id != B.Id || A.SchemaVersion != B.SchemaVersion ||
        A.ProgramKind != B.ProgramKind)
        return "shader-program-identity-mismatch";
    if (
        A.AllowedPermutationFlags != B.AllowedPermutationFlags ||
        A.Stages.size() != B.Stages.size() ||
        A.Variants.size() != B.Variants.size() ||
        A.RequiredParameters.size() != B.RequiredParameters.size() ||
        A.InterfaceBindings.size() != B.InterfaceBindings.size() ||
        A.ConstantRanges.size() != B.ConstantRanges.size())
        return "shader-program-shape-mismatch";
    if (!EqualDependencies(A.Dependencies, B.Dependencies))
        return "shader-program-dependency-mismatch";
    for (Core::usize Index = 0; Index < A.Stages.size(); ++Index)
    {
        const auto& X = A.Stages[Index];
        const auto& Y = B.Stages[Index];
        if (X.Stage != Y.Stage || X.EntryPoint != Y.EntryPoint ||
            X.Language != Y.Language ||
            X.Source.GetId() != Y.Source.GetId() ||
            X.ExpectedDigest != Y.ExpectedDigest)
            return "shader-program-stage-mismatch";
    }
    for (Core::usize Index = 0; Index < A.RequiredParameters.size(); ++Index)
        if (A.RequiredParameters[Index].Name != B.RequiredParameters[Index].Name ||
            A.RequiredParameters[Index].Type != B.RequiredParameters[Index].Type)
        {
            std::cout << "[DETAIL] shader parameter mismatch id="
                      << A.Id.ToString().CStr() << " index=" << Index
                      << " development-name="
                      << A.RequiredParameters[Index].Name.CStr()
                      << " development-type="
                      << static_cast<unsigned int>(
                             A.RequiredParameters[Index].Type)
                      << " cooked-name="
                      << B.RequiredParameters[Index].Name.CStr()
                      << " cooked-type="
                      << static_cast<unsigned int>(
                             B.RequiredParameters[Index].Type)
                      << '\n';
            return "shader-program-parameter-mismatch";
        }
    for (Core::usize Index = 0; Index < A.InterfaceBindings.size(); ++Index)
        if (!EqualShaderBinding(
                A.InterfaceBindings[Index], B.InterfaceBindings[Index]))
            return "shader-program-binding-mismatch";
    for (Core::usize Index = 0; Index < A.ConstantRanges.size(); ++Index)
        if (!EqualShaderRange(A.ConstantRanges[Index], B.ConstantRanges[Index]))
            return "shader-program-range-mismatch";
    for (Core::usize Variant = 0; Variant < A.Variants.size(); ++Variant)
    {
        const auto& X = A.Variants[Variant];
        const auto& Y = B.Variants[Variant];
        if (X.VariantName != Y.VariantName ||
            X.Permutation != Y.Permutation ||
            X.Payloads.size() != Y.Payloads.size())
            return "shader-program-variant-mismatch";
        for (Core::usize Payload = 0; Payload < X.Payloads.size(); ++Payload)
        {
            const auto& XP = X.Payloads[Payload];
            const auto& YP = Y.Payloads[Payload];
            if (XP.Stage != YP.Stage || XP.EntryPoint != YP.EntryPoint ||
                XP.Permutation != YP.Permutation ||
                XP.Payload.GetId() != YP.Payload.GetId())
                return "shader-program-payload-reference-mismatch";
        }
    }
    return nullptr;
}

const char* MaterialMismatch(
    const FMaterialAsset& Left,
    const FMaterialAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    if (A.Id != B.Id || A.SchemaVersion != B.SchemaVersion)
        return "material-identity-mismatch";
    if (A.Domain != B.Domain || A.BlendMode != B.BlendMode ||
        A.RenderState != B.RenderState)
        return "material-render-state-mismatch";
    if (A.Shader.GetId() != B.Shader.GetId() ||
        A.PermutationRequest != B.PermutationRequest)
        return "material-shader-mismatch";
    if (A.Parameters != B.Parameters)
        return "material-parameter-mismatch";
    if (!EqualDependencies(A.Dependencies, B.Dependencies))
    {
        std::cout << "[DETAIL] material dependency mismatch id="
                  << A.Id.ToString().CStr() << '\n';
        const auto Print = [](const char* Label,
                              const Core::TArray<FAssetDependency>& Values)
        {
            for (const auto& Value : Values)
                std::cout << "[DETAIL] " << Label << " target="
                          << Value.TargetId.ToString().CStr() << " role="
                          << static_cast<unsigned int>(Value.Role)
                          << " strength="
                          << static_cast<unsigned int>(Value.Strength)
                          << " resolution="
                          << static_cast<unsigned int>(Value.Resolution)
                          << '\n';
        };
        Print("development", A.Dependencies);
        Print("cooked", B.Dependencies);
        return "material-dependency-mismatch";
    }
    return nullptr;
}

bool EqualMaterialInstance(
    const FMaterialInstanceAsset& Left,
    const FMaterialInstanceAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    const auto ParentId = [](const FMaterialParentReference& Parent)
    {
        return std::visit(
            [](const auto& Reference)
            {
                return Reference.GetId();
            }, Parent.Reference);
    };
    return A.Id == B.Id && A.SchemaVersion == B.SchemaVersion &&
        ParentId(A.Parent) == ParentId(B.Parent) &&
        A.Overrides == B.Overrides &&
        EqualDependencies(A.Dependencies, B.Dependencies);
}

bool FiniteNear(float A, float B)
{
    return std::isfinite(A) && std::isfinite(B) &&
        Core::FMath::IsNearlyEqual(A, B, Core::FMath::DefaultTolerance);
}

bool EqualVector(const Core::FVector2& A, const Core::FVector2& B)
{
    return FiniteNear(A.X, B.X) && FiniteNear(A.Y, B.Y);
}

bool EqualVector(const Core::FVector3& A, const Core::FVector3& B)
{
    return FiniteNear(A.X, B.X) && FiniteNear(A.Y, B.Y) &&
        FiniteNear(A.Z, B.Z);
}

bool EqualVector(const Core::FVector4& A, const Core::FVector4& B)
{
    return FiniteNear(A.X, B.X) && FiniteNear(A.Y, B.Y) &&
        FiniteNear(A.Z, B.Z) && FiniteNear(A.W, B.W);
}

template <typename T>
bool EqualVectors(
    const Core::TArray<T>& A,
    const Core::TArray<T>& B,
    Core::uint32& Compared)
{
    if (A.size() != B.size()) return false;
    for (Core::usize Index = 0; Index < A.size(); ++Index)
    {
        if (!EqualVector(A[Index], B[Index])) return false;
        ++Compared;
    }
    return true;
}

bool EqualBounds(const FStaticMeshBounds& A, const FStaticMeshBounds& B)
{
    return A.IsValid() && B.IsValid() &&
        EqualVector(A.Box.Min, B.Box.Min) &&
        EqualVector(A.Box.Max, B.Box.Max) &&
        EqualVector(A.Sphere.Center, B.Sphere.Center) &&
        FiniteNear(A.Sphere.Radius, B.Sphere.Radius);
}

bool EqualMesh(
    const FStaticMeshAsset& Left,
    const FStaticMeshAsset& Right,
    Core::uint32& Compared)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    if (A.Id != B.Id || A.SchemaVersion != B.SchemaVersion ||
        A.Primitives.size() != B.Primitives.size() ||
        A.MaterialSlots.size() != B.MaterialSlots.size() ||
        !EqualBounds(A.Bounds, B.Bounds) ||
        !EqualDependencies(A.Dependencies, B.Dependencies))
        return false;
    for (Core::usize Slot = 0; Slot < A.MaterialSlots.size(); ++Slot)
        if (A.MaterialSlots[Slot].StableKey != B.MaterialSlots[Slot].StableKey ||
            A.MaterialSlots[Slot].Material.GetId() !=
                B.MaterialSlots[Slot].Material.GetId())
            return false;
    for (Core::usize Index = 0; Index < A.Primitives.size(); ++Index)
    {
        const auto& X = A.Primitives[Index];
        const auto& Y = B.Primitives[Index];
        if (X.StableKey != Y.StableKey ||
            X.MaterialSlotIndex != Y.MaterialSlotIndex ||
            X.SourcePrimitiveIndex != Y.SourcePrimitiveIndex ||
            !EqualBounds(X.LocalBounds, Y.LocalBounds) ||
            !EqualVectors(X.Vertices.Positions, Y.Vertices.Positions, Compared) ||
            !EqualVectors(X.Vertices.Normals, Y.Vertices.Normals, Compared) ||
            !EqualVectors(X.Vertices.Tangents, Y.Vertices.Tangents, Compared) ||
            !EqualVectors(X.Vertices.TexCoords[0], Y.Vertices.TexCoords[0], Compared) ||
            !EqualVectors(X.Vertices.TexCoords[1], Y.Vertices.TexCoords[1], Compared) ||
            X.Indices.GetIndexCount() != Y.Indices.GetIndexCount())
            return false;
        for (Core::uint32 Item = 0; Item < X.Indices.GetIndexCount(); ++Item)
        {
            if (X.Indices.GetIndex(Item) != Y.Indices.GetIndex(Item))
                return false;
            ++Compared;
        }
    }
    return true;
}

bool EqualModel(const FStaticModelAsset& Left, const FStaticModelAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    if (A.Id != B.Id || A.SchemaVersion != B.SchemaVersion ||
        A.SceneStableKey != B.SceneStableKey ||
        A.bSourceDefaultScene != B.bSourceDefaultScene ||
        A.RootNodeIndices != B.RootNodeIndices ||
        A.Nodes.size() != B.Nodes.size() ||
        !EqualBounds(A.Bounds, B.Bounds) ||
        !EqualDependencies(A.Dependencies, B.Dependencies))
        return false;
    for (Core::usize Index = 0; Index < A.Nodes.size(); ++Index)
    {
        const auto& X = A.Nodes[Index];
        const auto& Y = B.Nodes[Index];
        if (X.StableKey != Y.StableKey || X.DisplayName != Y.DisplayName ||
            !X.LocalTransform.IsFinite() || !Y.LocalTransform.IsFinite() ||
            !X.LocalTransform.ToMatrix().NearlyEquals(
                Y.LocalTransform.ToMatrix(), Core::FMath::DefaultTolerance) ||
            X.Children != Y.Children ||
            X.SourceNodeIndex != Y.SourceNodeIndex ||
            X.bNegativeDeterminant != Y.bNegativeDeterminant ||
            X.Mesh.has_value() != Y.Mesh.has_value() ||
            (X.Mesh && X.Mesh->GetId() != Y.Mesh->GetId()))
            return false;
    }
    return true;
}

Core::uint32 SourceChannels(EImageTexelFormat Format)
{
    switch (Format)
    {
    case EImageTexelFormat::R8_UNorm: return 1;
    case EImageTexelFormat::R8G8_UNorm: return 2;
    case EImageTexelFormat::R8G8B8_UNorm: return 3;
    case EImageTexelFormat::R8G8B8A8_UNorm: return 4;
    default: return 0;
    }
}

struct FTextureTolerance
{
    double MeanAbsoluteError = 0.0;
    Core::uint32 MaximumAbsoluteError = 0;
};

FTextureTolerance Tolerance(
    ETextureSemantic Semantic,
    EKTX2BasisModel BasisModel)
{
    if (BasisModel == EKTX2BasisModel::None) return {};
    switch (Semantic)
    {
    case ETextureSemantic::Color: return {24.0, 160};
    case ETextureSemantic::Normal: return {16.0, 128};
    case ETextureSemantic::Data: return {8.0, 32};
    case ETextureSemantic::Unspecified: return {};
    }
    return {};
}

const char* TextureMismatch(
    const FTextureAsset& Source,
    const FKTX2TextureArtifact& Cooked,
    const FAssetTargetProfileEvidence& Target,
    Core::uint64& ComparedSamples)
{
    const auto& Info = Cooked.GetInfo();
    if (Source.GetId() != Cooked.GetId() ||
        Source.GetSemantic() != Info.Semantic ||
        Source.GetColorSpace() != Info.ColorSpace ||
        Source.GetAlphaMode() != Info.AlphaMode ||
        Source.GetOrigin() != Info.Origin ||
        Source.GetMipPolicy() != Info.MipPolicy ||
        Source.GetContentDigest() != Info.ContentDigest ||
        Source.GetMips().size() != Info.Levels.size())
        return "texture-contract-mismatch";
    FAssetCookedTargetDecision Decision;
    if (ResolveAssetCookedTargetDecision(
            EAssetCookedFamily::ImageTexture,
            Cooked,
            Target.Profile,
            Decision) != EAssetResult::Success ||
        Decision.Validate() != EAssetResult::Success)
        return "texture-target-decision-mismatch";

    Core::TArray<Core::uint8> ArtifactBytes(
        Cooked.GetBytes().begin(), Cooked.GetBytes().end());
    FKTX2TextureArtifact Clone;
    if (FKTX2TextureArtifact::Create(
        Cooked.GetId(), Info, std::move(ArtifactBytes), Clone) !=
        EAssetResult::Success)
        return "texture-artifact-reopen-failed";

    if (Info.CompressionPolicy == ETextureCompressionPolicy::Uncompressed)
    {
        if (!Info.StoredTexelFormat ||
            *Info.StoredTexelFormat != Source.GetMips().front().GetFormat())
            return "texture-uncompressed-format-mismatch";
        const auto Bytes = Clone.GetBytes();
        for (Core::usize MipIndex = 0;
             MipIndex < Source.GetMips().size(); ++MipIndex)
        {
            const FImageMip& SourceMip = Source.GetMips()[MipIndex];
            const FKTX2Level& Level = Info.Levels[MipIndex];
            const auto SourceBytes = SourceMip.GetBytes();
            if (SourceMip.GetFormat() != *Info.StoredTexelFormat ||
                SourceMip.GetExtent() != Level.Extent ||
                Level.ByteOffset > Bytes.size() ||
                Level.ByteLength > Bytes.size() - Level.ByteOffset ||
                Level.ByteLength != SourceBytes.size())
                return "texture-uncompressed-layout-mismatch";
            const auto StoredBytes = Bytes.subspan(
                static_cast<Core::usize>(Level.ByteOffset),
                static_cast<Core::usize>(Level.ByteLength));
            if (!std::equal(
                    SourceBytes.begin(), SourceBytes.end(),
                    StoredBytes.begin(), StoredBytes.end()))
                return "texture-uncompressed-content-mismatch";
            ComparedSamples += SourceBytes.size();
        }
        return nullptr;
    }

    const ETextureTranscodeFormat Format =
        Source.GetColorSpace() == EImageColorSpace::SRGB
        ? ETextureTranscodeFormat::R8G8B8A8_SRGB
        : ETextureTranscodeFormat::R8G8B8A8_UNorm;
    FTextureTranscodeRequest Request;
    Request.Artifact = Core::MakeShared<FKTX2TextureArtifact>(
        std::move(Clone));
    Request.TargetFormat = Format;
    const FTextureTranscodeResult Transcoded =
        FTextureTranscoder::Transcode(Request);
    if (Transcoded.Result != EAssetResult::Success ||
        !Transcoded.Payload ||
        Transcoded.Payload->Mips.size() != Source.GetMips().size())
    {
        std::cout << "[DETAIL] texture transcode failed id="
                  << Source.GetId().ToString().CStr()
                  << " basis="
                  << static_cast<unsigned int>(Info.BasisModel)
                  << " target-format="
                  << static_cast<unsigned int>(Format)
                  << " result="
                  << static_cast<unsigned int>(Transcoded.Result)
                  << " source-mips=" << Source.GetMips().size()
                  << " target-mips="
                  << (Transcoded.Payload
                          ? Transcoded.Payload->Mips.size()
                          : 0)
                  << '\n';
        for (const FAssetDiagnostic& Diagnostic : Transcoded.Diagnostics)
            std::cout << "[DETAIL] transcode diagnostic code="
                      << Diagnostic.Code.CStr()
                      << " field=" << Diagnostic.Field.CStr()
                      << " reason=" << Diagnostic.Reason.CStr() << '\n';
        return "texture-transcode-failed";
    }

    Core::uint64 ErrorSum = 0;
    Core::uint32 MaximumError = 0;
    Core::uint64 Samples = 0;
    for (Core::usize MipIndex = 0;
         MipIndex < Source.GetMips().size(); ++MipIndex)
    {
        const FImageMip& SourceMip = Source.GetMips()[MipIndex];
        const auto& TargetMip = Transcoded.Payload->Mips[MipIndex];
        const Core::uint32 Channels = SourceChannels(SourceMip.GetFormat());
        const auto SourceBytes = SourceMip.GetBytes();
        if (Channels == 0 || TargetMip.BytesPerBlock != 4 ||
            TargetMip.BlockWidth != 1 || TargetMip.BlockHeight != 1 ||
            SourceMip.GetExtent() != TargetMip.Extent ||
            SourceBytes.size() / Channels * 4 != TargetMip.Bytes.size())
            return "texture-mip-layout-mismatch";
        const Core::uint64 Pixels = SourceBytes.size() / Channels;
        for (Core::uint64 Pixel = 0; Pixel < Pixels; ++Pixel)
            for (Core::uint32 Channel = 0; Channel < Channels; ++Channel)
            {
                const Core::uint32 Difference = static_cast<Core::uint32>(
                    std::abs(
                        static_cast<int>(SourceBytes[Pixel * Channels + Channel]) -
                        static_cast<int>(TargetMip.Bytes[Pixel * 4 + Channel])));
                ErrorSum += Difference;
                MaximumError = std::max(MaximumError, Difference);
                ++Samples;
            }
    }
    const FTextureTolerance Allowed = Tolerance(
        Source.GetSemantic(), Info.BasisModel);
    const double MeanError = Samples == 0
        ? std::numeric_limits<double>::infinity()
        : static_cast<double>(ErrorSum) / static_cast<double>(Samples);
    ComparedSamples += Samples;
    if (MeanError > Allowed.MeanAbsoluteError ||
        MaximumError > Allowed.MaximumAbsoluteError)
    {
        std::cout << "[DETAIL] texture tolerance mismatch id="
                  << Source.GetId().ToString().CStr()
                  << " semantic="
                  << static_cast<unsigned int>(Source.GetSemantic())
                  << " basis="
                  << static_cast<unsigned int>(Info.BasisModel)
                  << " mean=" << MeanError
                  << " allowed-mean=" << Allowed.MeanAbsoluteError
                  << " max=" << MaximumError
                  << " allowed-max=" << Allowed.MaximumAbsoluteError
                  << " samples=" << Samples << '\n';
        return "texture-tolerance-exceeded";
    }
    return nullptr;
}

bool ComparePayload(
    const FAssetPayload& Development,
    const FAssetPayload& Cooked,
    const FAssetTargetProfileEvidence& Target,
    FProductionAssetEquivalenceReport& Out)
{
    if (Development.GetAssetType() != Cooked.GetAssetType())
        return Fail(Out, "asset-type-mismatch");
    if (const auto* A = dynamic_cast<const FImageAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FImageAsset*>(&Cooked);
        if (!B || A->GetId() != B->GetId() ||
            A->GetBaseMip().GetExtent() != B->GetBaseMip().GetExtent() ||
            A->GetBaseMip().GetFormat() != B->GetBaseMip().GetFormat() ||
            A->GetColorSpace() != B->GetColorSpace() ||
            A->GetAlphaMode() != B->GetAlphaMode() ||
            A->GetOrigin() != B->GetOrigin() ||
            A->GetContentDigest() != B->GetContentDigest())
            return Fail(Out, "image-semantic-mismatch");
        return true;
    }
    if (const auto* A = dynamic_cast<const FTextureAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FKTX2TextureArtifact*>(&Cooked);
        if (!B) return Fail(Out, "texture-payload-type-mismatch");
        if (const char* Reason = TextureMismatch(
                *A, *B, Target, Out.ComparedTextureSamples))
            return Fail(Out, Reason);
        return true;
    }
    if (const auto* A = dynamic_cast<const FShaderSourceAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FShaderSourceAsset*>(&Cooked);
        if (!B || A->GetId() != B->GetId() ||
            A->GetLanguage() != B->GetLanguage() ||
            A->GetBytes() != B->GetBytes())
            return Fail(Out, "shader-source-mismatch");
        return true;
    }
    if (const auto* A = dynamic_cast<const FShaderPayloadAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FShaderPayloadAsset*>(&Cooked);
        const bool Common = B && A->GetId() == B->GetId() &&
            A->GetStage() == B->GetStage() &&
            A->GetEntryPoint() == B->GetEntryPoint() &&
            A->GetPermutation() == B->GetPermutation();
        const bool TargetCorrect = B &&
            ((Target.Profile.GraphicsBackend == EAssetGraphicsBackend::Vulkan &&
              B->GetBackend() == EShaderBackendFamily::Vulkan &&
              B->GetFormat() == EShaderPayloadFormat::SPIRV &&
              A->GetBytes() == B->GetBytes()) ||
             (Target.Profile.GraphicsBackend == EAssetGraphicsBackend::Metal &&
              B->GetBackend() == EShaderBackendFamily::Metal &&
              B->GetFormat() == EShaderPayloadFormat::MetalLibrary &&
              B->GetNativeBindingEvidence() &&
              B->GetNativeLibraryEvidence()));
        if (!Common || !TargetCorrect)
            return Fail(Out, "shader-target-mismatch");
        return true;
    }
    if (const auto* A = dynamic_cast<const FShaderAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FShaderAsset*>(&Cooked);
        if (!B) return Fail(Out, "shader-program-payload-type-mismatch");
        if (const char* Reason = ShaderProgramMismatch(*A, *B))
            return Fail(Out, Reason);
        return true;
    }
    if (const auto* A = dynamic_cast<const FMaterialAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FMaterialAsset*>(&Cooked);
        if (!B) return Fail(Out, "material-payload-type-mismatch");
        if (const char* Reason = MaterialMismatch(*A, *B))
            return Fail(Out, Reason);
        return true;
    }
    if (const auto* A = dynamic_cast<const FMaterialInstanceAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FMaterialInstanceAsset*>(&Cooked);
        if (!B || !EqualMaterialInstance(*A, *B))
            return Fail(Out, "material-instance-mismatch");
        return true;
    }
    if (const auto* A = dynamic_cast<const FStaticMeshAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FStaticMeshAsset*>(&Cooked);
        if (!B || !EqualMesh(*A, *B, Out.ComparedGeometryValues))
            return Fail(Out, "mesh-semantic-mismatch");
        return true;
    }
    if (const auto* A = dynamic_cast<const FStaticModelAsset*>(&Development))
    {
        const auto* B = dynamic_cast<const FStaticModelAsset*>(&Cooked);
        if (!B || !EqualModel(*A, *B))
            return Fail(Out, "model-semantic-mismatch");
        return true;
    }
    return Fail(Out, "unsupported-payload-family");
}

} // namespace

bool CompareProductionAssetClosures(
    const FProductionAssetClosure& Development,
    const FProductionAssetClosure& Cooked,
    const Stoner::Asset::FAssetTargetProfileEvidence& Target,
    FProductionAssetEquivalenceReport& Out)
{
    Out = {};
    if (Target.Validate() != Stoner::Asset::EAssetResult::Success ||
        Development.Entries.size() != Cooked.Entries.size())
        return Fail(Out, "closure-shape-mismatch");
    for (const auto& DevelopmentEntry : Development.Entries)
    {
        const auto* CookedEntry = Cooked.Find(DevelopmentEntry.AssetId);
        if (!CookedEntry ||
            DevelopmentEntry.AssetType != CookedEntry->AssetType ||
            !DevelopmentEntry.GetPayload() || !CookedEntry->GetPayload() ||
            !ComparePayload(
                *DevelopmentEntry.GetPayload(),
                *CookedEntry->GetPayload(),
                Target, Out))
            return false;
        ++Out.ComparedAssets;
    }
    return true;
}
