#include "FShaderProgramValidator.h"

#include <algorithm>
#include <limits>
#include <set>
#include <tuple>

namespace Stoner::Asset
{
namespace
{

void Diagnose(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    const FAssetId& Subject,
    const char* Field,
    const char* Reason)
{
    if (!Diagnostics)
    {
        return;
    }
    Diagnostics->push_back({
        EAssetStage::Validate,
        Result,
        EAssetDiagnosticSeverity::Error,
        Core::FString("asset.shader.validate"),
        Subject.ToString(),
        {},
        Core::FString(Field),
        {},
        {},
        Core::FString(Reason),
        {}});
}

bool IsToken(const Core::FString& Text)
{
    if (Text.IsEmpty() || Text.Len() > 128)
    {
        return false;
    }
    for (const char Character : Text.View())
    {
        if (!((Character >= 'A' && Character <= 'Z') ||
              (Character >= 'a' && Character <= 'z') ||
              (Character >= '0' && Character <= '9') ||
              Character == '_' || Character == '-' || Character == '.'))
        {
            return false;
        }
    }
    return true;
}

bool IsStageSetValid(const FShaderAssetDesc& Desc)
{
    std::set<EShaderStage> Stages;
    for (const FShaderSourceReference& Source : Desc.Stages)
    {
        Stages.insert(Source.Stage);
    }
    return Desc.ProgramKind == EShaderProgramKind::Graphics
        ? Stages == std::set<EShaderStage>{
              EShaderStage::Vertex, EShaderStage::Fragment}
        : Stages == std::set<EShaderStage>{EShaderStage::Compute};
}

bool IsProgramStage(
    const FShaderAssetDesc& Desc,
    EShaderStage Stage)
{
    return std::any_of(
        Desc.Stages.begin(),
        Desc.Stages.end(),
        [Stage](const FShaderSourceReference& Source)
        {
            return Source.Stage == Stage;
        });
}

bool NormalizeVisibility(
    const FShaderAssetDesc& Desc,
    Core::TArray<EShaderStage>& Visibility)
{
    std::sort(Visibility.begin(), Visibility.end());
    return !Visibility.empty() &&
        std::adjacent_find(Visibility.begin(), Visibility.end()) ==
            Visibility.end() &&
        std::all_of(
            Visibility.begin(),
            Visibility.end(),
            [&Desc](EShaderStage Stage)
            {
                return IsProgramStage(Desc, Stage);
            });
}

} // namespace

namespace Private
{

EAssetResult ValidateShaderProgram(
    FShaderAssetDesc& Desc,
    FAssetDiagnosticList* Diagnostics)
{
    if (!Desc.Id.IsValid() ||
        Desc.Id.GetAssetType() != TAssetTypeTraits<FShaderAsset>::GetAssetType() ||
        Desc.Version.Validate() != EAssetResult::Success ||
        Desc.SchemaVersion != 1 ||
        Desc.Stages.empty() ||
        !IsStageSetValid(Desc))
    {
        Diagnose(
            Diagnostics,
            EAssetResult::InvalidShaderProgram,
            Desc.Id,
            "/stages",
            "program-stage-set");
        return EAssetResult::InvalidShaderProgram;
    }

    std::set<std::pair<EShaderStage, Core::FString>> SourceKeys;
    for (const FShaderSourceReference& Source : Desc.Stages)
    {
        if (!IsToken(Source.EntryPoint) ||
            Source.Source.IsEmpty() ||
            Source.Locator.IsEmpty() ||
            !Source.ExpectedDigest.IsAvailable() ||
            !SourceKeys.insert({Source.Stage, Source.EntryPoint}).second)
        {
            Diagnose(
                Diagnostics,
                EAssetResult::InvalidShaderProgram,
                Desc.Id,
                "/stages",
                "invalid-or-duplicate-stage");
            return EAssetResult::InvalidShaderProgram;
        }
    }
    std::sort(
        Desc.Stages.begin(),
        Desc.Stages.end(),
        [](const auto& Left, const auto& Right)
        {
            return std::tie(Left.Stage, Left.EntryPoint) <
                std::tie(Right.Stage, Right.EntryPoint);
        });

    std::sort(
        Desc.AllowedPermutationFlags.begin(),
        Desc.AllowedPermutationFlags.end());
    if (std::any_of(
            Desc.AllowedPermutationFlags.begin(),
            Desc.AllowedPermutationFlags.end(),
            [](const Core::FString& Flag) { return !IsToken(Flag); }) ||
        std::adjacent_find(
            Desc.AllowedPermutationFlags.begin(),
            Desc.AllowedPermutationFlags.end()) !=
            Desc.AllowedPermutationFlags.end())
    {
        return EAssetResult::InvalidShaderProgram;
    }
    const std::set<Core::FString> Allowed(
        Desc.AllowedPermutationFlags.begin(),
        Desc.AllowedPermutationFlags.end());
    std::set<Core::FString> VariantKeys;
    for (FShaderVariantDefinition& Variant : Desc.Variants)
    {
        std::sort(
            Variant.Permutation.Flags.begin(),
            Variant.Permutation.Flags.end());
        if (std::adjacent_find(
                Variant.Permutation.Flags.begin(),
                Variant.Permutation.Flags.end()) !=
                Variant.Permutation.Flags.end() ||
            std::any_of(
                Variant.Permutation.Flags.begin(),
                Variant.Permutation.Flags.end(),
                [&Allowed](const auto& Flag)
                {
                    return !Allowed.contains(Flag);
                }) ||
            !VariantKeys.insert(Variant.Permutation.ToString()).second)
        {
            return EAssetResult::InvalidShaderProgram;
        }
        std::set<std::tuple<
            EShaderBackendFamily,
            Core::FString,
            EShaderPayloadFormat,
            EShaderStage,
            Core::FString>> PayloadKeys;
        for (const FShaderPayloadReference& Payload : Variant.Payloads)
        {
            const auto Source = std::find_if(
                Desc.Stages.begin(),
                Desc.Stages.end(),
                [&Payload](const FShaderSourceReference& Candidate)
                {
                    return Candidate.Stage == Payload.Stage &&
                        Candidate.EntryPoint == Payload.EntryPoint;
                });
            if (Payload.Payload.IsEmpty() ||
                !IsToken(Payload.Profile) ||
                !IsToken(Payload.EntryPoint) ||
                !IsToken(Payload.Producer) ||
                !IsToken(Payload.ProducerVersion) ||
                !Payload.ExpectedDigest.IsAvailable() ||
                Payload.Permutation != Variant.Permutation ||
                Source == Desc.Stages.end() ||
                !PayloadKeys.insert({
                    Payload.Backend,
                    Payload.Profile,
                    Payload.Format,
                    Payload.Stage,
                    Payload.EntryPoint}).second)
            {
                return EAssetResult::InvalidShaderProgram;
            }
        }
        std::sort(
            Variant.Payloads.begin(),
            Variant.Payloads.end(),
            [](const auto& Left, const auto& Right)
            {
                return std::tie(
                           Left.Backend,
                           Left.Profile,
                           Left.Format,
                           Left.Stage,
                           Left.EntryPoint,
                           *Left.Payload.GetId()) <
                    std::tie(
                           Right.Backend,
                           Right.Profile,
                           Right.Format,
                           Right.Stage,
                           Right.EntryPoint,
                           *Right.Payload.GetId());
            });
    }
    std::sort(
        Desc.Variants.begin(),
        Desc.Variants.end(),
        [](const auto& Left, const auto& Right)
        {
            const Core::FString LeftKey = Left.Permutation.ToString();
            const Core::FString RightKey = Right.Permutation.ToString();
            return LeftKey != RightKey
                ? LeftKey < RightKey
                : Left.VariantName < Right.VariantName;
        });

    std::set<Core::FString> RequiredNames;
    for (const FShaderRequiredParameter& Parameter : Desc.RequiredParameters)
    {
        if (!IsToken(Parameter.Name) ||
            !RequiredNames.insert(Parameter.Name).second)
        {
            return EAssetResult::InvalidShaderProgram;
        }
    }
    std::sort(
        Desc.RequiredParameters.begin(),
        Desc.RequiredParameters.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.Name < Right.Name;
        });

    std::set<std::pair<Core::uint32, Core::uint32>> BindingKeys;
    for (FShaderInterfaceBinding& Binding : Desc.InterfaceBindings)
    {
        if (Binding.ArrayCount == 0 ||
            !NormalizeVisibility(Desc, Binding.Visibility) ||
            (!Binding.Name.IsEmpty() && !IsToken(Binding.Name)) ||
            !BindingKeys.insert(
                {Binding.SetIndex, Binding.BindingIndex}).second)
        {
            return EAssetResult::InvalidShaderProgram;
        }
    }
    std::sort(
        Desc.InterfaceBindings.begin(),
        Desc.InterfaceBindings.end(),
        [](const auto& Left, const auto& Right)
        {
            return std::tie(
                       Left.SetIndex,
                       Left.BindingIndex,
                       Left.Kind,
                       Left.Name) <
                std::tie(
                       Right.SetIndex,
                       Right.BindingIndex,
                       Right.Kind,
                       Right.Name);
        });
    for (std::size_t Left = 0; Left < Desc.ConstantRanges.size(); ++Left)
    {
        FShaderConstantRange& A = Desc.ConstantRanges[Left];
        if (A.SizeBytes == 0 ||
            A.OffsetBytes >
                std::numeric_limits<Core::uint32>::max() - A.SizeBytes ||
            !NormalizeVisibility(Desc, A.Visibility))
        {
            return EAssetResult::InvalidShaderProgram;
        }
        for (std::size_t Right = Left + 1;
             Right < Desc.ConstantRanges.size();
             ++Right)
        {
            const FShaderConstantRange& B = Desc.ConstantRanges[Right];
            const Core::uint64 AEnd =
                static_cast<Core::uint64>(A.OffsetBytes) + A.SizeBytes;
            const Core::uint64 BEnd =
                static_cast<Core::uint64>(B.OffsetBytes) + B.SizeBytes;
            if (A.OffsetBytes < BEnd && B.OffsetBytes < AEnd)
            {
                for (const EShaderStage Stage : A.Visibility)
                {
                    if (std::find(
                            B.Visibility.begin(),
                            B.Visibility.end(),
                            Stage) != B.Visibility.end())
                    {
                        return EAssetResult::InvalidShaderProgram;
                    }
                }
            }
        }
    }
    std::sort(
        Desc.ConstantRanges.begin(),
        Desc.ConstantRanges.end(),
        [](const auto& Left, const auto& Right)
        {
            return std::tie(
                       Left.OffsetBytes,
                       Left.SizeBytes,
                       Left.Visibility) <
                std::tie(
                       Right.OffsetBytes,
                       Right.SizeBytes,
                       Right.Visibility);
        });
    return EAssetResult::Success;
}

} // namespace Private

EAssetResult FShaderAsset::CreateValidated(
    FShaderAssetDesc Desc,
    FShaderAsset& OutAsset,
    FAssetDiagnosticList* Diagnostics)
{
    OutAsset = {};
    const EAssetResult Result =
        Private::ValidateShaderProgram(Desc, Diagnostics);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }
    OutAsset.Desc_ = std::move(Desc);
    return EAssetResult::Success;
}

Core::FString FShaderAsset::GetAssetType() const
{
    return TAssetTypeTraits<FShaderAsset>::GetAssetType();
}

const FShaderAssetDesc& FShaderAsset::GetDesc() const noexcept
{
    return Desc_;
}

} // namespace Stoner::Asset
