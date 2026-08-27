#include "MetalResourceTests.h"

#include "MetalRHI/FMetalDeviceFactory.h"
#include "Core/SGPlatform.h"
#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIBufferUploadDesc.h"
#include "RHI/FRHISamplerDesc.h"
#include "RHI/FRHITextureDesc.h"
#include "RHI/FRHITextureUploadDesc.h"
#include "RHI/IRHIBuffer.h"
#include "RHI/IRHISampler.h"
#include "RHI/IRHITexture.h"

#include <array>
#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Record(FMetalResourceTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestBuffers(FMetalResourceTestResult& Result)
{
#if SG_PLATFORM_MAC
    auto Created = CreateMetalDevice();
    const std::array<uint8, 8> Bytes = {16, 32, 48, 64, 80, 96, 112, 128};
    bool Passed = Created.Succeeded();
    if (!Created.Succeeded())
    {
        Record(Result, Created.Result == ERHIResult::Unavailable,
            "buffer native checks are controlled unavailable without a device");
        return;
    }
    for (ERHIMemoryAccess Access : {
             ERHIMemoryAccess::HostVisible, ERHIMemoryAccess::DeviceLocal})
    {
        FRHIBufferDesc Desc;
        Desc.SizeInBytes = 32;
        Desc.Usage = ERHIBufferUsage::CopySource |
            ERHIBufferUsage::CopyDestination;
        Desc.MemoryAccess = Access;
        auto Buffer = Created.Device->CreateBuffer(Desc);
        FRHIBufferUploadDesc Upload{4, Bytes.data(), Bytes.size()};
        TArray<uint8> Readback;
        Passed = Passed && Buffer.Succeeded() &&
            Created.Device->UploadBuffer(Buffer.Object, Upload) ==
                ERHIResult::Success;
        for (Core::uint32 Iteration = 0; Iteration < 32 && Passed; ++Iteration)
        {
            Passed = ReadMetalBufferForValidation(
                    Created.Device, Buffer.Object, 4, Bytes.size(), Readback) ==
                    ERHIResult::Success &&
                Readback.size() == Bytes.size() &&
                std::equal(Bytes.begin(), Bytes.end(), Readback.begin());
        }
        Passed = Passed && Buffer.Object->Upload(Bytes.data(), 64, 0) ==
                ERHIResult::InvalidState &&
            Buffer.Object->Invalidate() == ERHIResult::Success &&
            Buffer.Object->Invalidate() == ERHIResult::InvalidState;
    }
    Record(Result, Passed,
        "host-visible and private buffers preserve repeated GPU readbacks");
#else
    Record(Result, true, "Metal buffer implementation is excluded off macOS");
#endif
}

void TestTextureAndSampler(FMetalResourceTestResult& Result)
{
#if SG_PLATFORM_MAC
    auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        Record(Result, Created.Result == ERHIResult::Unavailable,
            "texture native checks are controlled unavailable without a device");
        return;
    }
    FRHITextureDesc Desc;
    Desc.Width = 2;
    Desc.Height = 2;
    Desc.Format = ERHIFormat::R8G8B8A8_UNorm;
    Desc.Usage = ERHITextureUsage::Sampled |
        ERHITextureUsage::CopyDestination |
        ERHITextureUsage::CopySource;
    auto Texture = Created.Device->CreateTexture(Desc);
    const std::array<uint8, 16> Pixels = {
        255, 0, 0, 255, 0, 255, 0, 255,
        0, 0, 255, 255, 255, 255, 255, 255};
    FRHITextureUploadDesc Upload;
    Upload.Width = 2;
    Upload.Height = 2;
    Upload.RowPitchBytes = 8;
    Upload.Data = Pixels.data();
    Upload.DataSizeBytes = Pixels.size();
    FRHISamplerDesc SamplerDesc;
    auto Sampler = Created.Device->CreateSampler(SamplerDesc);
    Record(Result,
        Created.Succeeded() && Texture.Succeeded() &&
            Created.Device->UploadTexture(Texture.Object, Upload) ==
                ERHIResult::Success &&
            Texture.Object->GetFormat() == ERHIFormat::R8G8B8A8_UNorm &&
            Sampler.Succeeded() &&
            Sampler.Object->GetLifecycleState() ==
                ERHIResourceLifecycleState::Valid &&
            Sampler.Object->Invalidate() == ERHIResult::Success,
        "texture upload repacks rows and sampler owns native lifecycle");

    Desc.Width = 0;
    const auto Invalid = Created.Device->CreateTexture(Desc);
    Record(Result,
        !Invalid.Succeeded() && Invalid.Result == ERHIResult::InvalidState,
        "invalid texture descriptions fail before native allocation");
#else
    Record(Result, true, "Metal texture implementation is excluded off macOS");
#endif
}

} // namespace

FMetalResourceTestResult RunMetalResourceTests()
{
    FMetalResourceTestResult Result;
    TestBuffers(Result);
    TestTextureAndSampler(Result);
    return Result;
}
