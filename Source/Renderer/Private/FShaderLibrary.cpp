#include "Renderer/FShaderLibrary.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

namespace
{

bool ContainsString(const Stoner::Core::TArray<Stoner::Core::FString>& Values, const Stoner::Core::FString& Value)
{
    return std::find(Values.begin(), Values.end(), Value) != Values.end();
}

bool HasDuplicateString(Stoner::Core::TArray<Stoner::Core::FString> Values)
{
    std::sort(Values.begin(), Values.end());
    return std::adjacent_find(Values.begin(), Values.end()) != Values.end();
}

void SortUniqueStrings(Stoner::Core::TArray<Stoner::Core::FString>& Values)
{
    std::sort(Values.begin(), Values.end());
    Values.erase(std::unique(Values.begin(), Values.end()), Values.end());
}

} // namespace

EMaterialResult FShaderLibrary::RegisterShaderRecord(FShaderRecord Record, FMaterialDiagnosticLog* Diagnostics)
{
    if (Record.ShaderId.IsEmpty())
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ShaderLibrary,
                EMaterialResult::ValidationFailed, "MAT-SHADER-ID-EMPTY", "<empty>", "shader record id is required");
        }
        return EMaterialResult::ValidationFailed;
    }
    if (FindRecord(Record.ShaderId) != nullptr)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ShaderLibrary,
                EMaterialResult::DuplicateName, "MAT-SHADER-DUPLICATE", Record.ShaderId, "duplicate shader record id");
        }
        return EMaterialResult::DuplicateName;
    }

    if (HasDuplicateString(Record.AllowedPermutationFlags))
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Permutation,
                EMaterialResult::DuplicateName, "MAT-SHADER-FLAG-DUPLICATE", Record.ShaderId,
                "duplicate allowed permutation flag");
        }
        return EMaterialResult::DuplicateName;
    }
    for (const Stoner::Core::FString& Flag : Record.AllowedPermutationFlags)
    {
        if (Flag.IsEmpty())
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Permutation,
                    EMaterialResult::ValidationFailed, "MAT-SHADER-FLAG-EMPTY", Record.ShaderId,
                    "allowed permutation flag cannot be empty");
            }
            return EMaterialResult::ValidationFailed;
        }
    }

    SortUniqueStrings(Record.AllowedPermutationFlags);
    Stoner::Core::TArray<Stoner::Core::FString> VariantIds;
    Stoner::Core::TArray<Stoner::Core::FString> VariantKeys;
    for (const FShaderVariant& Variant : Record.Variants)
    {
        if (Variant.VariantId.IsEmpty())
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ShaderLibrary,
                    EMaterialResult::ValidationFailed, "MAT-SHADER-VARIANT-ID-EMPTY", Record.ShaderId,
                    "shader variant id is required");
            }
            return EMaterialResult::ValidationFailed;
        }
        for (const Stoner::Core::FString& Flag : Variant.Permutation.GetFlags())
        {
            if (!ContainsString(Record.AllowedPermutationFlags, Flag))
            {
                if (Diagnostics != nullptr)
                {
                    Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Permutation,
                        EMaterialResult::ValidationFailed, "MAT-SHADER-VARIANT-FLAG", Record.ShaderId,
                        Stoner::Core::FString(std::string("variant uses undeclared permutation flag: ") + Flag.ToStdString()));
                }
                return EMaterialResult::ValidationFailed;
            }
        }
        VariantIds.push_back(Variant.VariantId);
        VariantKeys.push_back(Variant.Permutation.GetCanonicalKey());
    }
    if (HasDuplicateString(VariantIds))
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ShaderLibrary,
                EMaterialResult::DuplicateName, "MAT-SHADER-VARIANT-DUPLICATE", Record.ShaderId,
                "duplicate shader variant id");
        }
        return EMaterialResult::DuplicateName;
    }
    if (HasDuplicateString(VariantKeys))
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Permutation,
                EMaterialResult::DuplicateName, "MAT-SHADER-VARIANT-KEY-DUPLICATE", Record.ShaderId,
                "duplicate shader variant permutation key");
        }
        return EMaterialResult::DuplicateName;
    }

    std::sort(Record.Variants.begin(), Record.Variants.end(), [](const FShaderVariant& Left, const FShaderVariant& Right) {
        if (Left.Permutation.GetCanonicalKey() == Right.Permutation.GetCanonicalKey())
        {
            return Left.VariantId < Right.VariantId;
        }
        return Left.Permutation.GetCanonicalKey() < Right.Permutation.GetCanonicalKey();
    });
    std::sort(Record.RequiredParameters.begin(), Record.RequiredParameters.end(), [](const FShaderRequiredParameter& Left, const FShaderRequiredParameter& Right) {
        return Left.Name < Right.Name;
    });

    Records.push_back(std::move(Record));
    std::sort(Records.begin(), Records.end(), [](const FShaderRecord& Left, const FShaderRecord& Right) {
        return Left.ShaderId < Right.ShaderId;
    });
    return EMaterialResult::Success;
}

const FShaderRecord* FShaderLibrary::FindRecord(const Stoner::Core::FString& ShaderId) const noexcept
{
    for (const FShaderRecord& Record : Records)
    {
        if (Record.ShaderId == ShaderId)
        {
            return &Record;
        }
    }
    return nullptr;
}

EMaterialResult FShaderLibrary::ResolveVariant(const Stoner::Core::FString& ShaderId, const FShaderPermutation& Permutation,
    const FShaderVariant*& OutVariant, FMaterialDiagnosticLog* Diagnostics) const
{
    OutVariant = nullptr;
    const FShaderRecord* Record = FindRecord(ShaderId);
    if (Record == nullptr)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ShaderLibrary,
                EMaterialResult::NotFound, "MAT-SHADER-NOT-FOUND", ShaderId, "shader record is not registered");
        }
        return EMaterialResult::NotFound;
    }
    if (Record->bInvalidated)
    {
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Invalidation,
                EMaterialResult::Invalidated, "MAT-SHADER-INVALIDATED", ShaderId, "shader record is invalidated");
        }
        return EMaterialResult::Invalidated;
    }

    for (const Stoner::Core::FString& Flag : Permutation.GetFlags())
    {
        if (!ContainsString(Record->AllowedPermutationFlags, Flag))
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Permutation,
                    EMaterialResult::ValidationFailed, "MAT-PERMUTATION-UNKNOWN-FLAG", ShaderId,
                    Stoner::Core::FString(std::string("unknown permutation flag: ") + Flag.ToStdString()));
            }
            return EMaterialResult::ValidationFailed;
        }
    }

    const Stoner::Core::FString RequestedKey = Permutation.GetCanonicalKey();
    for (const FShaderVariant& Variant : Record->Variants)
    {
        if (Variant.Permutation.GetCanonicalKey() == RequestedKey)
        {
            OutVariant = &Variant;
            return EMaterialResult::Success;
        }
    }

    if (Diagnostics != nullptr)
    {
        Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::ShaderLibrary,
            EMaterialResult::NotFound, "MAT-SHADER-VARIANT-NOT-FOUND", ShaderId,
            Stoner::Core::FString(std::string("missing shader variant for permutation: ") + RequestedKey.ToStdString()));
    }
    return EMaterialResult::NotFound;
}

EMaterialResult FShaderLibrary::ValidateRequiredParameters(const FShaderRecord& Record, const FMaterialParameterSet& Parameters,
    FMaterialDiagnosticLog* Diagnostics) const
{
    for (const FShaderRequiredParameter& Required : Record.RequiredParameters)
    {
        const FMaterialParameter* Parameter = Parameters.FindParameter(Required.Name);
        if (Parameter == nullptr)
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                    EMaterialResult::NotFound, "MAT-SHADER-PARAM-MISSING", Required.Name,
                    "shader required parameter is missing from material");
            }
            return EMaterialResult::NotFound;
        }
        if (Parameter->Value.Type != Required.Type)
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EMaterialDiagnosticSeverity::Error, EMaterialDiagnosticCategory::Parameter,
                    EMaterialResult::TypeMismatch, "MAT-SHADER-PARAM-TYPE", Required.Name,
                    "shader required parameter type does not match material parameter type");
            }
            return EMaterialResult::TypeMismatch;
        }
    }
    return EMaterialResult::Success;
}

void FShaderLibrary::InvalidateRecord(const Stoner::Core::FString& ShaderId)
{
    for (FShaderRecord& Record : Records)
    {
        if (Record.ShaderId == ShaderId)
        {
            Record.bInvalidated = true;
            return;
        }
    }
}

void FShaderLibrary::Clear()
{
    Records.clear();
}

const Stoner::Core::TArray<FShaderRecord>& FShaderLibrary::GetRecords() const noexcept
{
    return Records;
}

Stoner::Core::FString FShaderLibrary::Dump() const
{
    std::ostringstream Stream;
    Stream << "ShaderLibrary\n";
    for (const FShaderRecord& Record : Records)
    {
        Stream << "  Shader " << Record.ShaderId.CStr() << " name=" << Record.DiagnosticsName.CStr()
            << " invalidated=" << (Record.bInvalidated ? "true" : "false") << '\n';
        Stream << "    AllowedFlags=";
        for (std::size_t Index = 0; Index < Record.AllowedPermutationFlags.size(); ++Index)
        {
            if (Index > 0)
            {
                Stream << ',';
            }
            Stream << Record.AllowedPermutationFlags[Index].CStr();
        }
        Stream << '\n';
        for (const FShaderVariant& Variant : Record.Variants)
        {
            Stream << "    Variant " << Variant.VariantId.CStr()
                << " key=" << Variant.Permutation.GetCanonicalKey().CStr()
                << " stages=" << Variant.StageSummary.CStr() << '\n';
        }
        for (const FShaderRequiredParameter& Required : Record.RequiredParameters)
        {
            Stream << "    Required " << Required.Name.CStr() << ':' << ToString(Required.Type) << '\n';
        }
    }
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Renderer
