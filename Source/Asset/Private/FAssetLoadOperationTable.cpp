#include "FAssetLoadOperationTable.h"

#include <algorithm>

namespace Stoner::Asset::Private
{

EAssetOperationAttachResult FAssetLoadOperationTable::Attach(
    const FAssetLoadKey& Key,
    FAssetRequestHandle Request,
    Core::TSharedPtr<FSharedAssetLoadOperation>& OutOperation)
{
    OutOperation.reset();
    const auto Found = std::find_if(
        Operations_.begin(), Operations_.end(),
        [&Key](const auto& Value)
        {
            return Value && Value->Key == Key;
        });
    if (Found == Operations_.end())
    {
        OutOperation = Core::MakeShared<FSharedAssetLoadOperation>();
        OutOperation->Key = Key;
        OutOperation->Cancellation =
            Core::MakeShared<FAssetCancellationToken>();
        OutOperation->Interests.push_back(Request);
        Operations_.push_back(OutOperation);
        return EAssetOperationAttachResult::Created;
    }
    OutOperation = *Found;
    OutOperation->Interests.push_back(Request);
    if (!OutOperation->bTerminal)
        return EAssetOperationAttachResult::InFlight;
    return OutOperation->State == EAssetLoadOperationState::Ready &&
            OutOperation->Payload
        ? EAssetOperationAttachResult::ReadyCache
        : EAssetOperationAttachResult::TerminalFailure;
}

bool FAssetLoadOperationTable::Detach(
    const Core::TSharedPtr<FSharedAssetLoadOperation>& Operation,
    FAssetRequestHandle Request)
{
    if (!Operation) return false;
    const auto Found = std::find(
        Operation->Interests.begin(), Operation->Interests.end(), Request);
    if (Found == Operation->Interests.end()) return false;
    Operation->Interests.erase(Found);
    if (Operation->Interests.empty() && !Operation->bTerminal)
        Operation->Cancellation->RequestCancellation();
    ReclaimIfUnretained(Operation);
    return true;
}

void FAssetLoadOperationTable::ReclaimIfUnretained(
    const Core::TSharedPtr<FSharedAssetLoadOperation>& Operation)
{
    if (!Operation || !Operation->Interests.empty()) return;
    Operations_.erase(
        std::remove(Operations_.begin(), Operations_.end(), Operation),
        Operations_.end());
}

void FAssetLoadOperationTable::Clear()
{
    Operations_.clear();
}

Core::uint32 FAssetLoadOperationTable::Size() const noexcept
{
    return static_cast<Core::uint32>(Operations_.size());
}

Core::TArray<FAssetLoadOperationSnapshot>
FAssetLoadOperationTable::Inspect() const
{
    Core::TArray<FAssetLoadOperationSnapshot> Result;
    Result.reserve(Operations_.size());
    for (const auto& Operation : Operations_)
    {
        if (!Operation) continue;
        Result.push_back({Operation->Key,
            static_cast<Core::uint32>(Operation->Interests.size()),
            Operation->Result, Operation->FailurePath});
    }
    return Result;
}

} // namespace Stoner::Asset::Private
