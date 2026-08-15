#pragma once

#include "Asset/FAssetRequestHandle.h"
#include "Asset/FAssetPayload.h"
#include "Core/TSharedPtr.h"
#include "Core/FPlatformTypes.h"
#include "Core/TUniquePtr.h"

namespace Stoner::Asset::Private
{

class FAssetRequestTable
{
public:
    FAssetRequestTable(Core::uint64 ManagerLifetime, Core::uint32 Capacity);
    ~FAssetRequestTable();
    FAssetRequestTable(const FAssetRequestTable&) = delete;
    FAssetRequestTable& operator=(const FAssetRequestTable&) = delete;

    [[nodiscard]] bool Allocate(FAssetRequestHandle& OutHandle);
    [[nodiscard]] bool Transition(
        FAssetRequestHandle Handle,
        EAssetRequestState Expected,
        EAssetRequestState Next);
    [[nodiscard]] bool CommitTerminal(
        FAssetRequestHandle Handle,
        EAssetRequestState TerminalState,
        EAssetResult Result);
    [[nodiscard]] bool CommitReady(
        FAssetRequestHandle Handle,
        Core::TSharedPtr<const FAssetPayload> Payload);
    [[nodiscard]] bool GetPayload(
        FAssetRequestHandle Handle,
        Core::TSharedPtr<const FAssetPayload>& OutPayload) const;
    [[nodiscard]] bool Query(
        FAssetRequestHandle Handle,
        FAssetRequestSnapshot& OutSnapshot) const;
    [[nodiscard]] bool SetCompletionSequence(
        FAssetRequestHandle Handle,
        Core::uint64 Sequence);
    [[nodiscard]] bool Release(FAssetRequestHandle Handle);
    void DropPayloads();

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
};

} // namespace Stoner::Asset::Private
