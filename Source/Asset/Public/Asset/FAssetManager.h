#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetManagerConfig.h"
#include "Asset/FAssetRequestHandle.h"
#include "Asset/TAssetHandle.h"
#include "Core/TSharedPtr.h"
#include "Core/TUniquePtr.h"

#include <memory>
#include <utility>

namespace Stoner::Asset
{

struct FAssetManagerInspection;

class FAssetManager
{
public:
    FAssetManager(const FAssetManager&) = delete;
    FAssetManager& operator=(const FAssetManager&) = delete;
    ~FAssetManager();

    [[nodiscard]] static EAssetResult Create(
        const FAssetManagerConfig& Config,
        Core::TSharedPtr<FAssetManager>& OutManager,
        FAssetDiagnosticList& OutDiagnostics);

    template <CTypedAssetPayload T>
    [[nodiscard]] EAssetResult Request(
        const FAssetId& Id,
        FAssetRequestHandle& OutRequest,
        FAssetCompletionCallback Completion = {})
    {
        return RequestUntyped(
            Id, TAssetTypeTraits<T>::GetAssetType(), OutRequest,
            std::move(Completion));
    }

    [[nodiscard]] EAssetResult Query(
        FAssetRequestHandle Request,
        FAssetRequestSnapshot& OutSnapshot) const;

    template <CTypedAssetPayload T>
    [[nodiscard]] EAssetResult GetResult(
        FAssetRequestHandle Request,
        TAssetHandle<T>& OutHandle) const
    {
        Core::TSharedPtr<Private::FAssetHandleControl> Control;
        const EAssetResult Result = GetResultUntyped(
            Request, TAssetTypeTraits<T>::GetAssetType(), Control);
        if (Result != EAssetResult::Success)
        {
            OutHandle.Reset();
            return Result;
        }
        if (!Control ||
            dynamic_cast<const T*>(Control->GetPayload().get()) == nullptr)
        {
            OutHandle.Reset();
            return EAssetResult::TypeMismatch;
        }
        OutHandle = TAssetHandle<T>(std::move(Control));
        return EAssetResult::Success;
    }

    [[nodiscard]] EAssetResult Cancel(FAssetRequestHandle Request);
    [[nodiscard]] EAssetResult ReleaseRequest(FAssetRequestHandle Request);
    [[nodiscard]] FAssetPumpResult PumpCompletions(Core::uint32 MaxCount);
    [[nodiscard]] EAssetResult Shutdown();
    [[nodiscard]] FAssetManagerInspection Inspect() const;

private:
    struct FImpl;
    explicit FAssetManager(Core::TUniquePtr<FImpl> Impl);

    [[nodiscard]] EAssetResult RequestUntyped(
        const FAssetId& Id,
        const Core::FString& ExpectedType,
        FAssetRequestHandle& OutRequest,
        FAssetCompletionCallback Completion);
    [[nodiscard]] EAssetResult GetResultUntyped(
        FAssetRequestHandle Request,
        const Core::FString& ExpectedType,
        Core::TSharedPtr<Private::FAssetHandleControl>& OutControl) const;

    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Asset
