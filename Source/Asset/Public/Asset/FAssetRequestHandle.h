#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FPlatformTypes.h"

#include <functional>

namespace Stoner::Asset::Private
{
class FAssetRequestTable;
}

namespace Stoner::Asset
{

enum class EAssetRequestState : Core::uint8
{
    Invalid,
    Accepted,
    WaitingForDependencies,
    Loading,
    Ready,
    Failed,
    Cancelled
};

class FAssetRequestHandle
{
public:
    FAssetRequestHandle() = default;
    [[nodiscard]] bool IsValid() const noexcept
    {
        return ManagerLifetime_ != 0 && Generation_ != 0;
    }
    [[nodiscard]] Core::uint64 GetManagerLifetime() const noexcept
    {
        return ManagerLifetime_;
    }
    [[nodiscard]] Core::uint32 GetSlot() const noexcept { return Slot_; }
    [[nodiscard]] Core::uint32 GetGeneration() const noexcept
    {
        return Generation_;
    }
    [[nodiscard]] bool operator==(const FAssetRequestHandle&) const = default;

private:
    friend class Private::FAssetRequestTable;
    FAssetRequestHandle(
        Core::uint64 ManagerLifetime,
        Core::uint32 Slot,
        Core::uint32 Generation) noexcept
        : ManagerLifetime_(ManagerLifetime), Slot_(Slot), Generation_(Generation)
    {
    }

    Core::uint64 ManagerLifetime_ = 0;
    Core::uint32 Slot_ = 0;
    Core::uint32 Generation_ = 0;
};

struct FAssetRequestSnapshot
{
    FAssetRequestHandle Handle;
    EAssetRequestState State = EAssetRequestState::Invalid;
    EAssetResult Result = EAssetResult::NotReady;
    Core::uint64 CompletionSequence = 0;
};

using FAssetCompletionCallback =
    std::function<void(FAssetRequestHandle, EAssetResult)>;

struct FAssetPumpResult
{
    EAssetResult Result = EAssetResult::Success;
    Core::uint32 Dispatched = 0;
};

} // namespace Stoner::Asset
