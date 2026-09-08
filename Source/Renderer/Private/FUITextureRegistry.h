#pragma once
#include "Renderer/FUITextureLease.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHICommandBuffer.h"
#include "RHI/IRHIFence.h"
#include "Renderer/FRenderGraph.h"
#include <array>
#include <span>

namespace Stoner::Renderer
{
struct FUIGpuTextureContext
{
    const FRenderGraph* Graph = nullptr;
    FRenderGraphPassHandle Consumer;
    Stoner::Core::uint64 FrameId = 0, SettingsRevision = 0, DisplayGeneration = 0;
};
struct FUIGpuTextureRegistration
{
    Stoner::Core::uint32 LogicalSlot = 0;
    FUITextureId Previous;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> Texture;
    FRenderGraphResourceHandle Resource;
    FRenderGraphPassHandle Producer;
};
struct FUITextureRegistryStatistics
{
    Stoner::Core::uint32 Generations = 0;
    Stoner::Core::uint64 GPUBytes = 0;
    Stoner::Core::uint64 CPUShadowBytes = 0;
    Stoner::Core::uint64 StagingBytes = 0;
};

// One command-stream reservation. Destroy before submission to cancel it.
// After SubmitDeferred succeeds, Commit attaches the exact render fence;
// submission failure requires discarding the command before this ticket.
class FUITextureSubmission
{
public:
    FUITextureSubmission() = default;
    ~FUITextureSubmission();
    FUITextureSubmission(FUITextureSubmission&&) noexcept;
    FUITextureSubmission& operator=(FUITextureSubmission&&) noexcept;
    FUITextureSubmission(const FUITextureSubmission&) = delete;
    FUITextureSubmission& operator=(const FUITextureSubmission&) = delete;
    [[nodiscard]] Stoner::RHI::ERHIResult Commit(
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIFence>& Fence) noexcept;
private:
    friend class FUITextureRegistry;
    void Cancel() noexcept;
    struct FEntry
    {
        FUITextureLease Lease;
        Stoner::Core::uint32 UseIndex = 0;
    };
    Stoner::Core::TArray<FEntry> Entries;
    bool bCommitted = false;
};

// Owned and serviced on the render/session thread. Poll before resetting any
// completed frame fence. This registry accepts no presentation fence.
class FUITextureRegistry
{
public:
    explicit FUITextureRegistry(
        Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice> InDevice);
    FUITextureRegistry(const FUITextureRegistry&) = delete;
    FUITextureRegistry& operator=(const FUITextureRegistry&) = delete;
    void BeginEligibleFrame(Stoner::Core::uint64 FrameId, bool bEligible) noexcept;
    [[nodiscard]] FUITextureResult Prepare(const FUITextureRequest& Request);
    // Renderer-owned targets have no CPU request/acknowledgement or staging.
    [[nodiscard]] Stoner::RHI::ERHIResult RegisterGpuTexture(
        const FUIGpuTextureRegistration&, const FUIGpuTextureContext&, FUITextureId& OutId);
    [[nodiscard]] Stoner::RHI::ERHIResult RetireGpuTexture(FUITextureId) noexcept;
    [[nodiscard]] FUITextureLease Acquire(FUITextureId Id) const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult CanRecordSubmission(std::span<const FUITextureLease> Leases,
        const FUIGpuTextureContext* Context = nullptr) const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult RecordSubmission(
        std::span<const FUITextureLease> Leases,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHICommandBuffer>& Command,
        FUITextureSubmission& OutSubmission, const FUIGpuTextureContext* Context = nullptr);
    // Private Renderer binding lookup preserves registry/lease identity checks.
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> ResolveTexture(
        const FUITextureLease& Lease) const noexcept;
    void Poll() noexcept;
    [[nodiscard]] FUITextureRegistryStatistics GetStatistics() const noexcept;
private:
    [[nodiscard]] Stoner::Core::TSharedPtr<FUITextureRecord> Find(FUITextureId Id) const noexcept;
    Stoner::Core::TSharedPtr<Stoner::RHI::IRHIDevice> Device;
    std::array<Stoner::Core::TSharedPtr<FUITextureRecord>, 512> Records{};
    std::array<FUITextureId, 257> Current{};
    std::array<Stoner::Core::uint64, 257> LastGeneration{};
    Stoner::Core::uint64 LastFrame = 0, FramePayloadBytes = 0;
    Stoner::Core::uint32 FrameRequests = 0;
    bool bFrameEligible = false;
};
}
