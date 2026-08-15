#pragma once

#include "Asset/IAssetImporter.h"

#include <condition_variable>
#include <mutex>

namespace Stoner::Tests
{

class FAssetManagerControlledState
{
public:
    [[nodiscard]] bool WaitUntilStarted(Core::uint64 TimeoutMilliseconds);
    void Release();
    [[nodiscard]] int GetCalls() const noexcept;

private:
    friend class FAssetManagerControlledLoader;
    mutable std::mutex Mutex_;
    std::condition_variable Wake_;
    bool Started_ = false;
    bool Released_ = false;
    int Calls_ = 0;
};

class FAssetManagerControlledLoader final : public Asset::IAssetImporter
{
public:
    FAssetManagerControlledLoader(
        Asset::FAssetId OutputId,
        Core::TSharedPtr<FAssetManagerControlledState> State,
        bool Conforming = true);

    [[nodiscard]] Asset::FAssetExtensionCapability GetCapability() const override;
    [[nodiscard]] Asset::FAssetProbeResult Probe(
        const Asset::FAssetSourceDescriptor& Descriptor,
        std::span<const Core::uint8> Prefix) override;
    [[nodiscard]] Asset::EAssetResult Import(
        const Asset::FAssetSourceDescriptor& Descriptor,
        const Asset::FAssetSourceLease& Source,
        Core::TArray<Asset::FAssetImportOutput>& OutOutputs) override;
    [[nodiscard]] Asset::EAssetResult Import(
        const Asset::FAssetImportRequest& Request,
        Core::TArray<Asset::FAssetImportOutput>& OutOutputs) override;

private:
    Asset::FAssetId OutputId_;
    Core::TSharedPtr<FAssetManagerControlledState> State_;
    bool Conforming_ = true;
};

} // namespace Stoner::Tests

