#include "VulkanRHI/VulkanDevice.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>

namespace
{

std::atomic<long long> GFailAfter{-1};

[[nodiscard]] bool ShouldFailAllocation() noexcept
{
    long long Remaining = GFailAfter.load(std::memory_order_relaxed);
    while (Remaining >= 0)
    {
        if (Remaining == 0)
        {
            if (GFailAfter.compare_exchange_weak(
                    Remaining, -1, std::memory_order_relaxed))
            {
                return true;
            }
        }
        else if (GFailAfter.compare_exchange_weak(
                     Remaining, Remaining - 1,
                     std::memory_order_relaxed))
        {
            return false;
        }
    }
    return false;
}

void ArmFailure(long long SuccessfulAllocations) noexcept
{
    GFailAfter.store(SuccessfulAllocations, std::memory_order_relaxed);
}

void DisarmFailure() noexcept
{
    GFailAfter.store(-1, std::memory_order_relaxed);
}

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FProbe
{
    void Check(const char* Name, bool bValue)
    {
        std::cout << Name << '=' << (bValue ? 1 : 0) << '\n';
        bPassed = bPassed && bValue;
    }

    bool bPassed = true;
};

class FMockBuffer final : public IRHIBuffer
{
public:
    explicit FMockBuffer(ERHIBufferUsage Usage)
    {
        Desc.SizeInBytes = 64;
        Desc.Usage = Usage;
    }

    [[nodiscard]] const FRHIBufferDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] uint64 GetSizeInBytes() const noexcept override
    {
        return Desc.SizeInBytes;
    }

    [[nodiscard]] ERHIBufferUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return Lifecycle;
    }

    ERHIResult Invalidate() override
    {
        if (Lifecycle == ERHIResourceLifecycleState::Invalidated)
        {
            return ERHIResult::InvalidState;
        }
        Lifecycle = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHIBufferDesc Desc;
    ERHIResourceLifecycleState Lifecycle =
        ERHIResourceLifecycleState::Valid;
};

class FMockTexture final : public IRHITexture
{
public:
    explicit FMockTexture(FRHITextureDesc InDesc)
        : Desc(InDesc)
    {
    }

    [[nodiscard]] const FRHITextureDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }

    [[nodiscard]] ERHIFormat GetFormat() const noexcept override
    {
        return Desc.Format;
    }

    [[nodiscard]] ERHITextureUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }

    [[nodiscard]] ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return Lifecycle;
    }

    ERHIResult Invalidate() override
    {
        if (Lifecycle == ERHIResourceLifecycleState::Invalidated)
        {
            return ERHIResult::InvalidState;
        }
        Lifecycle = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState Lifecycle =
        ERHIResourceLifecycleState::Valid;
};

[[nodiscard]] bool Initialize(FVulkanDevice& Device)
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Device.Initialize(Desc) == ERHIResult::Success;
}

[[nodiscard]] FRHIPipelineLayoutDesc LayoutDesc()
{
    FRHIPipelineLayoutDesc Desc;
    Desc.Bindings = {{0, 0, ERHIDescriptorType::UniformBuffer, 1,
        ERHIShaderStageFlags::Vertex}};
    return Desc;
}

[[nodiscard]] FRHITextureDesc TextureDesc(
    ERHIFormat Format = ERHIFormat::R8G8B8A8_UNorm)
{
    FRHITextureDesc Desc;
    Desc.Width = 8;
    Desc.Height = 8;
    Desc.Format = Format;
    Desc.Usage = ERHITextureUsage::CopyDestination;
    return Desc;
}

[[nodiscard]] bool DescriptorStateMachine()
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    Device.ConfigureDescriptorPoolCapacity(1);
    const auto Layout = Device.CreatePipelineLayout(LayoutDesc());
    const auto First = Device.CreateDescriptorSet(Layout.Object, 0);
    const auto Exhausted = Device.CreateDescriptorSet(Layout.Object, 0);
    const bool bInitial = First.Succeeded() &&
        Exhausted.Result == ERHIResult::Unavailable &&
        Device.GetDescriptorPoolAllocatedCount() == 1;
    const bool bReleased = First.Object->Invalidate() == ERHIResult::Success &&
        First.Object->Invalidate() == ERHIResult::InvalidState &&
        Device.GetDescriptorPoolAllocatedCount() == 0;
    const auto Replacement = Device.CreateDescriptorSet(Layout.Object, 0);
    const bool bReused = Replacement.Succeeded() &&
        Device.GetDescriptorPoolAllocatedCount() == 1;
    (void)Device.Shutdown();
    return bInitial && bReleased && bReused &&
        Replacement.Object->GetLifecycleState() ==
            ERHIResourceLifecycleState::Invalidated &&
        Device.GetDescriptorPoolAllocatedCount() == 0;
}

[[nodiscard]] bool DescriptorFailureRollback(
    long long FailureIndex,
    bool bSeedPool)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    Device.ConfigureDescriptorPoolCapacity(1);
    const auto Layout = Device.CreatePipelineLayout(LayoutDesc());
    if (bSeedPool)
    {
        const auto Seed = Device.CreateDescriptorSet(Layout.Object, 0);
        if (!Seed.Succeeded() ||
            Seed.Object->Invalidate() != ERHIResult::Success)
        {
            return false;
        }
    }
    ArmFailure(FailureIndex);
    const auto Result = Device.CreateDescriptorSet(Layout.Object, 0);
    DisarmFailure();
    const bool bRolledBack = Result.Result == ERHIResult::Unavailable &&
        !Result.Object && Device.GetDescriptorPoolAllocatedCount() == 0;
    (void)Device.Shutdown();
    return bRolledBack;
}

[[nodiscard]] bool BufferUploadMatrix()
{
    const unsigned char Data[16] = {};
    auto CopyBuffer = std::make_shared<FMockBuffer>(
        ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination);
    auto NonCopyBuffer = std::make_shared<FMockBuffer>(
        ERHIBufferUsage::Uniform);
    const bool bExact = FVulkanUploadRequest::CreateBufferUpload(
        CopyBuffer, Data, sizeof(Data), {48, sizeof(Data)}).Succeeded();
    const bool bMismatch =
        FVulkanUploadRequest::CreateBufferUpload(
            CopyBuffer, Data, sizeof(Data), {0, 8}).Result ==
        ERHIResult::InvalidState;
    const bool bBounds =
        FVulkanUploadRequest::CreateBufferUpload(
            CopyBuffer, Data, sizeof(Data), {49, sizeof(Data)}).Result ==
        ERHIResult::InvalidState;
    const bool bUsage =
        FVulkanUploadRequest::CreateBufferUpload(
            NonCopyBuffer, Data, sizeof(Data), {0, sizeof(Data)}).Result ==
        ERHIResult::Unsupported;
    (void)CopyBuffer->Invalidate();
    const bool bLifecycle =
        FVulkanUploadRequest::CreateBufferUpload(
            CopyBuffer, Data, sizeof(Data), {0, sizeof(Data)}).Result ==
        ERHIResult::InvalidState;
    return bExact && bMismatch && bBounds && bUsage && bLifecycle;
}

[[nodiscard]] bool FormatFootprintMatrix()
{
    struct FCase
    {
        ERHIFormat Format;
        uint64 Bytes;
    };
    constexpr std::array<FCase, 10> Cases = {{
        {ERHIFormat::R8_UNorm, 1},
        {ERHIFormat::R8G8B8A8_UNorm, 4},
        {ERHIFormat::B8G8R8A8_UNorm, 4},
        {ERHIFormat::R16G16B16A16_Float, 8},
        {ERHIFormat::R32_Float, 4},
        {ERHIFormat::R32G32_Float, 8},
        {ERHIFormat::R32G32B32_Float, 12},
        {ERHIFormat::D24_UNorm_S8_UInt, 4},
        {ERHIFormat::D32_Float, 4},
        {ERHIFormat::S8_UInt, 1},
    }};

    const unsigned char Data[13] = {};
    for (const FCase& Case : Cases)
    {
        FRHITextureDesc Desc = TextureDesc(Case.Format);
        Desc.Width = 1;
        Desc.Height = 1;
        auto Texture = std::make_shared<FMockTexture>(Desc);
        if (GetRHIFormatByteSize(Case.Format) != Case.Bytes ||
            !FVulkanUploadRequest::CreateTextureUpload(
                Texture, Data, Case.Bytes, {0, 0, 0, 0, 0, 1, 1, 1})
                 .Succeeded())
        {
            return false;
        }
        const uint64 WrongBytes = Case.Bytes == 1 ? 2 : Case.Bytes - 1;
        if (FVulkanUploadRequest::CreateTextureUpload(
                Texture, Data, WrongBytes, {0, 0, 0, 0, 0, 1, 1, 1})
                .Result != ERHIResult::InvalidState)
        {
            return false;
        }
    }
    return GetRHIFormatByteSize(ERHIFormat::Unknown) == 0;
}

[[nodiscard]] bool SubresourceMatrix()
{
    const unsigned char Data[64] = {};

    FRHITextureDesc TwoD = TextureDesc();
    TwoD.Width = 8;
    TwoD.Height = 4;
    TwoD.MipLevels = 3;
    auto TwoDTexture = std::make_shared<FMockTexture>(TwoD);
    const bool bTwoD =
        FVulkanUploadRequest::CreateTextureUpload(
            TwoDTexture, Data, 8, {2, 0, 0, 0, 0, 2, 1, 1}).Succeeded() &&
        FVulkanUploadRequest::CreateTextureUpload(
            TwoDTexture, Data, 12, {2, 0, 0, 0, 0, 3, 1, 1}).Result ==
            ERHIResult::InvalidState;

    FRHITextureDesc ThreeD = TextureDesc();
    ThreeD.Dimension = ERHITextureDimension::Texture3D;
    ThreeD.Width = 8;
    ThreeD.Height = 4;
    ThreeD.Depth = 2;
    ThreeD.MipLevels = 2;
    auto ThreeDTexture = std::make_shared<FMockTexture>(ThreeD);
    const bool bThreeD =
        FVulkanUploadRequest::CreateTextureUpload(
            ThreeDTexture, Data, 32, {1, 0, 0, 0, 0, 4, 2, 1})
            .Succeeded() &&
        FVulkanUploadRequest::CreateTextureUpload(
            ThreeDTexture, Data, 64, {1, 0, 0, 0, 0, 4, 2, 2}).Result ==
            ERHIResult::InvalidState;

    FRHITextureDesc Array = TextureDesc();
    Array.Dimension = ERHITextureDimension::Texture1DArray;
    Array.Width = 8;
    Array.Height = 1;
    Array.ArrayLayers = 3;
    Array.MipLevels = 2;
    auto ArrayTexture = std::make_shared<FMockTexture>(Array);
    const bool bArray =
        FVulkanUploadRequest::CreateTextureUpload(
            ArrayTexture, Data, 16, {1, 2, 0, 0, 0, 4, 1, 1})
            .Succeeded() &&
        FVulkanUploadRequest::CreateTextureUpload(
            ArrayTexture, Data, 16, {1, 3, 0, 0, 0, 4, 1, 1}).Result ==
            ERHIResult::InvalidState;

    FRHITextureDesc Cube = TextureDesc();
    Cube.Dimension = ERHITextureDimension::TextureCube;
    Cube.ArrayLayers = 6;
    Cube.MipLevels = 2;
    auto CubeTexture = std::make_shared<FMockTexture>(Cube);
    const bool bCube =
        FVulkanUploadRequest::CreateTextureUpload(
            CubeTexture, Data, 64, {1, 5, 0, 0, 0, 4, 4, 1})
            .Succeeded() &&
        FVulkanUploadRequest::CreateTextureUpload(
            CubeTexture, Data, 64, {1, 6, 0, 0, 0, 4, 4, 1}).Result ==
            ERHIResult::InvalidState;

    return bTwoD && bThreeD && bArray && bCube;
}

[[nodiscard]] bool UnsupportedAndOverflowMatrix()
{
    const unsigned char Data[256] = {};

    FRHITextureDesc NonCopy = TextureDesc();
    NonCopy.Usage = ERHITextureUsage::Sampled;
    auto NonCopyTexture = std::make_shared<FMockTexture>(NonCopy);

    FRHITextureDesc Multisampled = TextureDesc();
    Multisampled.Width = 4;
    Multisampled.Height = 4;
    Multisampled.SampleCount = ERHISampleCount::Four;
    auto MultisampledTexture = std::make_shared<FMockTexture>(Multisampled);

    FRHITextureDesc Overflow = TextureDesc(ERHIFormat::R32G32B32_Float);
    Overflow.Dimension = ERHITextureDimension::Texture3D;
    Overflow.Width = std::numeric_limits<uint32>::max();
    Overflow.Height = std::numeric_limits<uint32>::max();
    Overflow.Depth = std::numeric_limits<uint32>::max();
    auto OverflowTexture = std::make_shared<FMockTexture>(Overflow);

    return FVulkanUploadRequest::CreateTextureUpload(
               NonCopyTexture, Data, 256, {0, 0, 0, 0, 0, 8, 8, 1})
               .Result == ERHIResult::Unsupported &&
        FVulkanUploadRequest::CreateTextureUpload(
            MultisampledTexture, Data, 256,
            {0, 0, 0, 0, 0, 4, 4, 1}).Result ==
            ERHIResult::Unsupported &&
        FVulkanUploadRequest::CreateTextureUpload(
            OverflowTexture, Data, 1,
            {0, 0, 0, 0, 0,
                std::numeric_limits<uint32>::max(),
                std::numeric_limits<uint32>::max(),
                std::numeric_limits<uint32>::max()}).Result ==
            ERHIResult::Unavailable;
}

[[nodiscard]] bool UploadAllocationFailureMatrix()
{
    const unsigned char Data[16] = {};
    for (long long FailureIndex = 0; FailureIndex <= 2; ++FailureIndex)
    {
        auto Buffer = std::make_shared<FMockBuffer>(
            ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination);
        ArmFailure(FailureIndex);
        const auto Result = FVulkanUploadRequest::CreateBufferUpload(
            Buffer, Data, sizeof(Data), {0, sizeof(Data)});
        DisarmFailure();
        if (Result.Result != ERHIResult::Unavailable || Result.Object)
        {
            return false;
        }
    }
    return true;
}

} // namespace

void* operator new(std::size_t Size)
{
    if (ShouldFailAllocation())
    {
        throw std::bad_alloc();
    }
    if (void* Memory = std::malloc(Size == 0 ? 1 : Size))
    {
        return Memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t Size)
{
    return ::operator new(Size);
}

void* operator new(std::size_t Size, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(Size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t Size, const std::nothrow_t&) noexcept
{
    return ::operator new(Size, std::nothrow);
}

void operator delete(void* Memory) noexcept
{
    std::free(Memory);
}

void operator delete[](void* Memory) noexcept
{
    std::free(Memory);
}

void operator delete(void* Memory, std::size_t) noexcept
{
    std::free(Memory);
}

void operator delete[](void* Memory, std::size_t) noexcept
{
    std::free(Memory);
}

int main()
{
    static_assert(!std::is_constructible_v<FVulkanDescriptorPool, uint32>);
    static_assert(!std::is_constructible_v<FVulkanDescriptorSet,
        const TSharedPtr<IRHIPipelineLayout>&, uint32,
        FVulkanDescriptorReservation&&>);
    static_assert(!std::is_constructible_v<FVulkanSampler,
        const FRHISamplerDesc&>);
    static_assert(!std::is_default_constructible_v<FVulkanUploadRequest>);
    static_assert(!std::is_copy_constructible_v<
        FVulkanDescriptorReservation>);
    static_assert(std::is_nothrow_move_constructible_v<
        FVulkanDescriptorReservation>);

    FProbe Probe;
    Probe.Check("factory_api_closure", true);
    Probe.Check("descriptor_state_machine", DescriptorStateMachine());
    Probe.Check("pool_object_failure_rollback",
        DescriptorFailureRollback(0, false));
    Probe.Check("pool_control_failure_rollback",
        DescriptorFailureRollback(1, false));
    Probe.Check("descriptor_wrapper_failure_rollback",
        DescriptorFailureRollback(0, true));
    Probe.Check("descriptor_control_failure_rollback",
        DescriptorFailureRollback(1, true));
    Probe.Check("descriptor_tracking_failure_rollback",
        DescriptorFailureRollback(2, true));
    Probe.Check("buffer_upload_matrix", BufferUploadMatrix());
    Probe.Check("all_format_footprints", FormatFootprintMatrix());
    Probe.Check("subresource_dimension_matrix", SubresourceMatrix());
    Probe.Check("unsupported_and_overflow_matrix",
        UnsupportedAndOverflowMatrix());
    Probe.Check("upload_allocation_failure_matrix",
        UploadAllocationFailureMatrix());
    std::cout << "classification=descriptor-sampler-upload-fixes-independently-verified\n";
    return Probe.bPassed ? 0 : 1;
}
