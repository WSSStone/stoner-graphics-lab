#pragma once

#include "FAssetRuntimeTypes.h"
#include "Asset/FAssetRequestHandle.h"
#include "Core/TSharedPtr.h"

#include <vector>

namespace Stoner::Asset::Private
{

struct FSharedAssetLoadOperation
{
    FAssetLoadKey Key;
    Core::TSharedPtr<FAssetCancellationToken> Cancellation;
    std::vector<FAssetRequestHandle> Interests;
    Core::TSharedPtr<const FAssetPayload> Payload;
    Core::TArray<FAssetId> FailurePath;
    EAssetResult Result = EAssetResult::NotReady;
    EAssetLoadOperationState State = EAssetLoadOperationState::Created;
    bool bTerminal = false;
};

enum class EAssetOperationAttachResult : Core::uint8
{
    Created,
    InFlight,
    ReadyCache,
    TerminalFailure
};

struct FAssetLoadOperationSnapshot
{
    FAssetLoadKey Key;
    Core::uint32 CallerInterests = 0;
    EAssetResult Result = EAssetResult::NotReady;
    Core::TArray<FAssetId> FailurePath;
};

class FAssetLoadOperationTable
{
public:
    [[nodiscard]] EAssetOperationAttachResult Attach(
        const FAssetLoadKey& Key,
        FAssetRequestHandle Request,
        Core::TSharedPtr<FSharedAssetLoadOperation>& OutOperation);
    [[nodiscard]] bool Detach(
        const Core::TSharedPtr<FSharedAssetLoadOperation>& Operation,
        FAssetRequestHandle Request);
    void ReclaimIfUnretained(
        const Core::TSharedPtr<FSharedAssetLoadOperation>& Operation);
    void Clear();
    [[nodiscard]] Core::uint32 Size() const noexcept;
    [[nodiscard]] Core::TArray<FAssetLoadOperationSnapshot> Inspect() const;

private:
    std::vector<Core::TSharedPtr<FSharedAssetLoadOperation>> Operations_;
};

} // namespace Stoner::Asset::Private
