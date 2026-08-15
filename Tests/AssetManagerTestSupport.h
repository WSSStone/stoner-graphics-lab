#pragma once

#include "Asset/AssetMinimal.h"

#include <atomic>
#include <mutex>

namespace Stoner::Asset
{

class FRuntimeTestPayload final : public FAssetPayload
{
public:
    explicit FRuntimeTestPayload(Core::FString Value)
        : Value_(std::move(Value))
    {
    }

    [[nodiscard]] Core::FString GetAssetType() const override
    {
        return Core::FString("RuntimeTest");
    }
    [[nodiscard]] const Core::FString& GetValue() const noexcept
    {
        return Value_;
    }

private:
    Core::FString Value_;
};

template <>
struct TAssetTypeTraits<FRuntimeTestPayload>
{
    [[nodiscard]] static Core::FString GetAssetType()
    {
        return Core::FString("RuntimeTest");
    }
};

} // namespace Stoner::Asset

struct FAssetManagerTestExtensions
{
    Stoner::Core::TSharedPtr<Stoner::Asset::FAssetExtensionRegistry> Registry;
    Stoner::Asset::FAssetRegistrationToken ResolverToken;
    Stoner::Asset::FAssetRegistrationToken ImporterToken;
    Stoner::Core::TSharedPtr<std::atomic<int>> ResolveCalls;
    Stoner::Core::TSharedPtr<std::atomic<int>> ImportCalls;
    Stoner::Core::TSharedPtr<std::atomic<bool>> MutateAfterImport;
};

[[nodiscard]] Stoner::Asset::FAssetId MakeRuntimeTestId(
    const char* LogicalPath = "Runtime/Main");
[[nodiscard]] Stoner::Core::TSharedPtr<const Stoner::Asset::FAssetTargetProfileEvidence>
LoadRuntimeTestTargetEvidence();
[[nodiscard]] FAssetManagerTestExtensions MakeRuntimeTestExtensions(
    const Stoner::Asset::FAssetId& Id,
    const char* Value = "runtime-payload");
[[nodiscard]] FAssetManagerTestExtensions MakeRuntimeImageExtensions(
    Stoner::Core::TArray<Stoner::Core::uint8> Bytes);
[[nodiscard]] Stoner::Asset::FAssetManagerConfig MakeDevelopmentManagerConfig(
    const FAssetManagerTestExtensions& Extensions);
[[nodiscard]] bool WaitForRequestTerminal(
    const Stoner::Asset::FAssetManager& Manager,
    Stoner::Asset::FAssetRequestHandle Request,
    Stoner::Asset::FAssetRequestSnapshot& OutSnapshot);
