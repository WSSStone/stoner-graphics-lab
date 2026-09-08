#include "FInteractiveLabShaders.h"
#include <algorithm>
#include <map>
#include <new>

namespace Stoner::Demo
{
namespace
{
class FLookup final : public Asset::IShaderPayloadLookup
{
public:
    std::map<Asset::FAssetId, Core::TSharedPtr<const Asset::FShaderPayloadAsset>> Payloads;
    Core::TSharedPtr<const Asset::FShaderPayloadAsset> Find(const Asset::FAssetId& Id) const override
    {
        const auto It = Payloads.find(Id);
        return It == Payloads.end() ? nullptr : It->second;
    }
};
}
Asset::EAssetResult PrepareInteractiveLabShaders(const FProductionContentLoadedClosure& Closure,
    const Asset::FAssetDigest& ExpectedGeneration,
    const Asset::FAssetTargetProfileEvidence& Target,
    FInteractiveLabShaders& OutShaders, Core::FString& OutReason)
{
    using namespace Asset;
    OutReason = "UI shader closure is unavailable in the selected immutable generation";
    if (!ExpectedGeneration.IsAvailable() || Closure.GenerationIdentity != ExpectedGeneration)
        return EAssetResult::InvalidInput;
    try
    {
        FLookup Lookup;
        for (const auto& Payload : Closure.RenderShaderPayloads)
            if (!Payload || !Lookup.Payloads.emplace(Payload->GetId(), Payload).second)
                return EAssetResult::InvalidInput;
        FShaderTargetRequest Request;
        if (Target.Profile.GraphicsBackend == EAssetGraphicsBackend::Metal)
            Request.Backend = EShaderBackendFamily::Metal;
        else if (Target.Profile.GraphicsBackend == EAssetGraphicsBackend::Vulkan)
            Request.Backend = EShaderBackendFamily::Vulkan;
        else return EAssetResult::InvalidInput;
        Request.CpuArchitecture = Target.Profile.CpuArchitecture;
        for (const auto& Choice : Target.Profile.ShaderPayloadChoices)
            if (Choice.Backend == Target.Profile.GraphicsBackend &&
                std::find(Request.AcceptableProfiles.begin(), Request.AcceptableProfiles.end(), Choice.Profile) ==
                    Request.AcceptableProfiles.end()) Request.AcceptableProfiles.push_back(Choice.Profile);
        FInteractiveLabShaders Candidate;
        Candidate.Generation = ExpectedGeneration;
        const auto Select = [&](const char* Path, Renderer::FShaderAssetSnapshot& Snapshot)
        {
            const FShaderAsset* Program = nullptr;
            for (const auto& Entry : Closure.RenderShaders)
                if (Entry && Entry->GetDesc().Id.GetLogicalPath() == Core::FString(Path))
                {
                    if (Program) return false;
                    Program = Entry.get();
                }
            FSelectedShaderProgram Selected;
            return Program && SelectShaderProgram(*Program, Request, Lookup, Selected) == EAssetResult::Success &&
                Renderer::ConvertShaderAsset({&Selected}, Snapshot) == Renderer::EMaterialResult::Success &&
                Snapshot.ModuleDescriptions.size() == 2;
        };
        if (!Select("Engine/Shaders/UI/UIDraw", Candidate.Draw) ||
            !Select("Engine/Shaders/UI/UICopy", Candidate.Copy)) return EAssetResult::TargetUnavailable;
        if (!Select("Engine/Shaders/UI/UIDiagnostic",Candidate.Diagnostic)) return EAssetResult::TargetUnavailable;
        OutShaders = std::move(Candidate);
        OutReason = {};
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutReason = "UI shader preflight allocation failed";
        return EAssetResult::CapacityExceeded;
    }
}
}
