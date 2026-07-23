#include "Renderer/FDeferredSurfaceData.h"

#include <array>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

[[nodiscard]] bool IsFinite(float Value) noexcept
{
    return Stoner::Core::FMath::IsFinite(Value);
}

} // namespace

bool FDeferredDepthPolicy::IsValid() const noexcept
{
    if (!IsFinite(NearPlane) || !IsFinite(FarPlane) || NearPlane <= 0.0f || FarPlane <= NearPlane)
    {
        return false;
    }
    if (Convention == EDeferredDepthConvention::StandardZ)
    {
        return FarClearValue == 1.0f && CompareOp == Stoner::RHI::ERHICompareOp::LessEqual;
    }
    return FarClearValue == 0.0f && CompareOp == Stoner::RHI::ERHICompareOp::GreaterEqual;
}

Stoner::Core::FString FDeferredDepthPolicy::GetIdentity() const
{
    std::ostringstream Stream;
    Stream << ToString(Convention) << ":near=" << NearPlane << ":far=" << FarPlane
        << ":clear=" << FarClearValue << ":compare="
        << (CompareOp == Stoner::RHI::ERHICompareOp::LessEqual ? "LessEqual" : "GreaterEqual");
    return Stoner::Core::FString(Stream.str());
}

bool FDeferredSurfaceLayout::IsValid(FDeferredDiagnosticLog* Diagnostics) const
{
    bool bValid = Extent.IsPositive() && SampleCount == Stoner::RHI::ERHISampleCount::One &&
        DepthPolicy.IsValid() && Attachments.size() == 4 && !LayoutId.IsEmpty();
    std::array<int, 7> SemanticCounts{};
    int DepthCount = 0;
    for (const FDeferredSurfaceAttachment& Attachment : Attachments)
    {
        if (Attachment.Name.IsEmpty() || Attachment.Format == Stoner::RHI::ERHIFormat::Unknown)
        {
            bValid = false;
        }
        if (Attachment.bDepth)
        {
            ++DepthCount;
            if (Attachment.Format != Stoner::RHI::ERHIFormat::D32_Float ||
                Attachment.ClearValue.X != DepthPolicy.FarClearValue)
            {
                bValid = false;
            }
        }
        for (EDeferredSurfaceSemantic Semantic : Attachment.Semantics)
        {
            ++SemanticCounts[static_cast<std::size_t>(Semantic)];
        }
    }
    for (int Count : SemanticCounts)
    {
        bValid = bValid && Count == 1;
    }
    bValid = bValid && DepthCount == 1;
    if (!bValid && Diagnostics)
    {
        Diagnostics->Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::SurfaceData,
            EDeferredResult::InvalidSurfaceLayout, "DEF-SURFACE-LAYOUT", LayoutId,
            "surface layout requires four single-sample attachments and every semantic exactly once");
    }
    return bValid;
}

const FDeferredSurfaceAttachment* FDeferredSurfaceLayout::FindAttachment(
    EDeferredSurfaceSemantic Semantic) const noexcept
{
    for (const FDeferredSurfaceAttachment& Attachment : Attachments)
    {
        for (EDeferredSurfaceSemantic Candidate : Attachment.Semantics)
        {
            if (Candidate == Semantic)
            {
                return &Attachment;
            }
        }
    }
    return nullptr;
}

FDeferredDepthPolicy MakeDeferredDepthPolicy(EDeferredDepthConvention Convention,
    float NearPlane, float FarPlane) noexcept
{
    FDeferredDepthPolicy Policy;
    Policy.Convention = Convention;
    Policy.NearPlane = NearPlane;
    Policy.FarPlane = FarPlane;
    Policy.FarClearValue = Convention == EDeferredDepthConvention::StandardZ ? 1.0f : 0.0f;
    Policy.CompareOp = Convention == EDeferredDepthConvention::StandardZ
        ? Stoner::RHI::ERHICompareOp::LessEqual
        : Stoner::RHI::ERHICompareOp::GreaterEqual;
    return Policy;
}

FDeferredSurfaceLayout MakeDefaultDeferredSurfaceLayout(FDeferredExtent2D Extent,
    EDeferredDepthConvention Convention, float NearPlane, float FarPlane)
{
    FDeferredSurfaceLayout Layout;
    Layout.Extent = Extent;
    Layout.DepthPolicy = MakeDeferredDepthPolicy(Convention, NearPlane, FarPlane);
    Layout.Attachments = {
        {"BaseColorAO", Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm,
            {EDeferredSurfaceSemantic::BaseColor, EDeferredSurfaceSemantic::AmbientOcclusion},
            Stoner::Core::FVector4(0.0f, 0.0f, 0.0f, 1.0f), false},
        {"NormalRoughness", Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
            {EDeferredSurfaceSemantic::WorldNormal, EDeferredSurfaceSemantic::Roughness},
            Stoner::Core::FVector4(0.0f, 0.0f, 1.0f, 1.0f), false},
        {"EmissiveMetallic", Stoner::RHI::ERHIFormat::R16G16B16A16_Float,
            {EDeferredSurfaceSemantic::Emissive, EDeferredSurfaceSemantic::Metallic},
            Stoner::Core::FVector4::Zero(), false},
        {"Depth", Stoner::RHI::ERHIFormat::D32_Float,
            {EDeferredSurfaceSemantic::Depth},
            Stoner::Core::FVector4(Layout.DepthPolicy.FarClearValue, 0.0f, 0.0f, 0.0f), true},
    };
    std::ostringstream Identity;
    Identity << "DeferredSurfaceV1:" << Extent.Width << 'x' << Extent.Height << ':'
        << Layout.DepthPolicy.GetIdentity().CStr();
    Layout.LayoutId = Identity.str();
    return Layout;
}

bool IsDeferredFinite(const Stoner::Core::FMatrix4x4& Value) noexcept
{
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Column = 0; Column < 4; ++Column)
        {
            if (!IsFinite(Value.M[Row][Column]))
            {
                return false;
            }
        }
    }
    return true;
}

bool TryBuildWorldNormalFromModel(const Stoner::Core::FMatrix4x4& Model,
    Stoner::Core::FMatrix4x4& OutWorldNormalFromModel) noexcept
{
    if (!IsDeferredFinite(Model) ||
        !Stoner::Core::FMath::IsNearlyZero(Model.M[3][0]) ||
        !Stoner::Core::FMath::IsNearlyZero(Model.M[3][1]) ||
        !Stoner::Core::FMath::IsNearlyZero(Model.M[3][2]) ||
        !Stoner::Core::FMath::IsNearlyEqual(Model.M[3][3], 1.0f))
    {
        OutWorldNormalFromModel = Stoner::Core::FMatrix4x4::Identity();
        return false;
    }

    const float A = Model.M[0][0], B = Model.M[0][1], C = Model.M[0][2];
    const float D = Model.M[1][0], E = Model.M[1][1], F = Model.M[1][2];
    const float G = Model.M[2][0], H = Model.M[2][1], I = Model.M[2][2];
    const float Determinant = A * (E * I - F * H) - B * (D * I - F * G) + C * (D * H - E * G);
    if (!IsFinite(Determinant) || Stoner::Core::FMath::Abs(Determinant) <= Stoner::Core::FMath::DefaultTolerance)
    {
        OutWorldNormalFromModel = Stoner::Core::FMatrix4x4::Identity();
        return false;
    }

    const float InverseDeterminant = 1.0f / Determinant;
    const float Inverse[3][3] = {
        {(E * I - F * H) * InverseDeterminant, (C * H - B * I) * InverseDeterminant, (B * F - C * E) * InverseDeterminant},
        {(F * G - D * I) * InverseDeterminant, (A * I - C * G) * InverseDeterminant, (C * D - A * F) * InverseDeterminant},
        {(D * H - E * G) * InverseDeterminant, (B * G - A * H) * InverseDeterminant, (A * E - B * D) * InverseDeterminant},
    };
    OutWorldNormalFromModel = Stoner::Core::FMatrix4x4::Identity();
    for (int Row = 0; Row < 3; ++Row)
    {
        for (int Column = 0; Column < 3; ++Column)
        {
            OutWorldNormalFromModel.M[Row][Column] = Inverse[Column][Row];
        }
    }
    return IsDeferredFinite(OutWorldNormalFromModel);
}

const char* ToString(EDeferredSurfaceSemantic Semantic) noexcept
{
    switch (Semantic)
    {
    case EDeferredSurfaceSemantic::BaseColor: return "BaseColor";
    case EDeferredSurfaceSemantic::WorldNormal: return "WorldNormal";
    case EDeferredSurfaceSemantic::Metallic: return "Metallic";
    case EDeferredSurfaceSemantic::Roughness: return "Roughness";
    case EDeferredSurfaceSemantic::Emissive: return "Emissive";
    case EDeferredSurfaceSemantic::AmbientOcclusion: return "AmbientOcclusion";
    case EDeferredSurfaceSemantic::Depth: return "Depth";
    }
    return "Unknown";
}

const char* ToString(EDeferredDepthConvention Convention) noexcept
{
    return Convention == EDeferredDepthConvention::StandardZ ? "StandardZ" : "ReversedZ";
}

} // namespace Stoner::Renderer
