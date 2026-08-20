#include "MetalNativeIntegrationTests.h"

#include "Asset/FAssetDigest.h"
#include "Core/SGPlatform.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/RHIMinimal.h"

#include <array>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::RHI;

std::string HexEncode(std::string_view Value)
{
    std::ostringstream Stream;
    Stream << std::hex << std::setfill('0');
    for (const unsigned char Byte : Value)
        Stream << std::setw(2) << static_cast<unsigned int>(Byte);
    return Stream.str();
}

void EmitNativeDeviceEvidence(const FMetalDeviceCreateResult& Created)
{
    const FString Capabilities = DumpRHIDeviceCapabilities(
        Created.Device->GetCapabilities());
    const auto CapabilityBytes = std::span<const uint8>(
        reinterpret_cast<const uint8*>(Capabilities.View().data()),
        Capabilities.Len());
    const Asset::FAssetDigest CapabilityDigest =
        Asset::FAssetDigest::FromBytes(CapabilityBytes);
    const auto& Adapter = Created.Diagnostics.SelectedAdapter;
    std::cout << "[EVIDENCE] metal-native-device identity=registry-"
              << Adapter.RegistryId
              << " name-utf8-hex=" << HexEncode(Adapter.Name.View())
              << " capability="
              << CapabilityDigest.ToLowerHex().ToStdString() << '\n';
}

void Record(
    FMetalNativeIntegrationTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

#if SG_PLATFORM_MAC
TRHIObjectResult<IRHIBuffer> CreateCopyBuffer(
    const TSharedPtr<IRHIDevice>& Device,
    uint64 Size)
{
    FRHIBufferDesc Desc;
    Desc.SizeInBytes = Size;
    Desc.Usage = ERHIBufferUsage::CopySource |
        ERHIBufferUsage::CopyDestination;
    Desc.MemoryAccess = ERHIMemoryAccess::HostVisible;
    return Device->CreateBuffer(Desc);
}

bool RunTransferAndSynchronization(const TSharedPtr<IRHIDevice>& Device)
{
    constexpr std::array<uint8, 16> Expected = {
        1, 3, 5, 7, 9, 11, 13, 15,
        17, 19, 21, 23, 25, 27, 29, 31};
    auto Source = CreateCopyBuffer(Device, Expected.size());
    auto Middle = CreateCopyBuffer(Device, Expected.size());
    auto Destination = CreateCopyBuffer(Device, Expected.size());
    auto Producer = Device->CreateCommandBuffer(ERHIQueueType::Transfer);
    auto Consumer = Device->CreateCommandBuffer(ERHIQueueType::Transfer);
    auto ProducerQueue = Device->CreateCommandQueue(ERHIQueueType::Transfer);
    auto ConsumerQueue = Device->CreateCommandQueue(ERHIQueueType::Transfer);
    auto Ready = Device->CreateSemaphore();
    auto Complete = Device->CreateFence();
    if (!Source.Succeeded() || !Middle.Succeeded() ||
        !Destination.Succeeded() || !Producer.Succeeded() ||
        !Consumer.Succeeded() || !ProducerQueue.Succeeded() ||
        !ConsumerQueue.Succeeded() || !Ready.Succeeded() ||
        !Complete.Succeeded())
        return false;

    const FRHIBufferUploadDesc Upload{0, Expected.data(), Expected.size()};
    const FRHIBufferCopyRange Copy{0, 0, Expected.size()};
    if (Device->UploadBuffer(Source.Object, Upload) != ERHIResult::Success ||
        Producer.Object->Begin() != ERHIResult::Success ||
        Producer.Object->RecordBufferCopy(
            Source.Object, Middle.Object, Copy) != ERHIResult::Success ||
        Producer.Object->End() != ERHIResult::Success ||
        Consumer.Object->Begin() != ERHIResult::Success ||
        Consumer.Object->RecordBufferCopy(
            Middle.Object, Destination.Object, Copy) != ERHIResult::Success ||
        Consumer.Object->End() != ERHIResult::Success)
        return false;

    // Logical invalidation rejects new work but recorded work retains native data.
    if (Source.Object->Invalidate() != ERHIResult::Success ||
        ProducerQueue.Object->Submit(
            Producer.Object, {}, {Ready.Object}, nullptr) !=
            ERHIResult::Success ||
        ConsumerQueue.Object->Submit(
            Consumer.Object, {Ready.Object}, {}, Complete.Object) !=
            ERHIResult::Success ||
        Complete.Object->Wait(5'000'000) != ERHIResult::Success)
        return false;

    TArray<uint8> Bytes;
    return ReadMetalBufferForValidation(
            Device, Destination.Object, 0, Expected.size(), Bytes) ==
            ERHIResult::Success &&
        Bytes.size() == Expected.size() &&
        std::equal(Expected.begin(), Expected.end(), Bytes.begin());
}

bool RunUnalignedTextureReadback(const TSharedPtr<IRHIDevice>& Device)
{
    constexpr std::array<uint8, 16> Expected = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255};
    FRHITextureDesc TextureDesc;
    TextureDesc.Width = 2;
    TextureDesc.Height = 2;
    TextureDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
    TextureDesc.Usage = ERHITextureUsage::CopySource |
        ERHITextureUsage::CopyDestination;
    auto Texture = Device->CreateTexture(TextureDesc);
    auto Destination = CreateCopyBuffer(Device, Expected.size());
    auto Commands = Device->CreateCommandBuffer(ERHIQueueType::Transfer);
    auto Queue = Device->CreateCommandQueue(ERHIQueueType::Transfer);
    auto Fence = Device->CreateFence();
    if (!Texture.Succeeded() || !Destination.Succeeded() ||
        !Commands.Succeeded() || !Queue.Succeeded() || !Fence.Succeeded())
        return false;

    FRHITextureUploadDesc Upload;
    Upload.Width = 2;
    Upload.Height = 2;
    Upload.RowPitchBytes = 8;
    Upload.Data = Expected.data();
    Upload.DataSizeBytes = Expected.size();
    FRHITextureBufferCopyRegion Region;
    Region.Width = 2;
    Region.Height = 2;
    return Device->UploadTexture(Texture.Object, Upload) == ERHIResult::Success &&
        Commands.Object->Begin() == ERHIResult::Success &&
        Commands.Object->RecordTextureToBufferCopy(
            Texture.Object, Destination.Object, Region) == ERHIResult::Success &&
        Commands.Object->End() == ERHIResult::Success &&
        Queue.Object->Submit(Commands.Object, {}, {}, Fence.Object) ==
            ERHIResult::Success &&
        Fence.Object->Wait(5'000'000) == ERHIResult::Success &&
        [&] {
            TArray<uint8> Bytes;
            return ReadMetalBufferForValidation(
                    Device, Destination.Object, 0, Expected.size(), Bytes) ==
                    ERHIResult::Success &&
                Bytes.size() == Expected.size() &&
                std::equal(Expected.begin(), Expected.end(), Bytes.begin());
        }();
}
#endif

} // namespace

FMetalNativeIntegrationTestResult
RunMetalNativeIntegrationTests(bool bRequireNative)
{
    FMetalNativeIntegrationTestResult Result;
#if SG_PLATFORM_MAC
    auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        Record(Result, !bRequireNative && Created.Result == ERHIResult::Unavailable,
            bRequireNative
                ? "required native Metal device is available"
                : "native Metal conformance is controlled unavailable");
        return Result;
    }
    EmitNativeDeviceEvidence(Created);
    Record(Result, RunTransferAndSynchronization(Created.Device),
        "native Metal queues preserve transfer ordering and retained resources");
    Record(Result, RunUnalignedTextureReadback(Created.Device),
        "native Metal normalizes unaligned texture readback rows");
    const auto Shutdown = Created.Device->Shutdown();
    FMetalBackendInspection Inspection;
    Record(Result,
        Shutdown == ERHIResult::Success &&
            InspectMetalDevice(Created.Device, Inspection) &&
            Inspection.InFlightSubmissionCount == 0 &&
            !Inspection.bAcceptingWork,
        "native Metal shutdown drains accepted work before invalidation");
#else
    Record(Result, !bRequireNative,
        bRequireNative
            ? "required native Metal is unavailable off macOS"
            : "native Metal conformance is excluded off macOS");
#endif
    return Result;
}
