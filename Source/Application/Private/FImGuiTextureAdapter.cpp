#include "FImGuiTextureAdapter.h"
#include "imgui.h"
#include <algorithm>
#include <limits>
#include <new>
#include <utility>

namespace Stoner::Application
{
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;
FImGuiTextureAdapter::FImGuiTextureAdapter(FPrepare InPrepare)
    : Prepare(std::move(InPrepare)), bFailed(!Prepare) {}
FImGuiTextureAdapter::~FImGuiTextureAdapter() { (void)Cancel(); }
ERHIResult FImGuiTextureAdapter::Cancel() noexcept
{
    bFailed = true;
    ERHIResult Status = ERHIResult::Success;
    for (auto& Binding : Bindings)
    {
        if (Binding.Id.IsValid() && Prepare)
        {
            FUITextureRequest Request;
            Request.RequestId = LastRequest == std::numeric_limits<uint64>::max() ? LastRequest : ++LastRequest;
            Request.Operation = EUITextureOperation::Destroy;
            Request.TextureId = Binding.Id; Request.ExpectedGeneration = Binding.Id.Generation;
            try
            {
                const auto Result = Prepare(Request);
                if (!Result.IsValid() || (Result.Result != ERHIResult::Success && Result.Result != ERHIResult::NotReady))
                    Status = ERHIResult::Unavailable;
            }
            catch (...) { Status = ERHIResult::Unavailable; }
        }
        // Closing cancels CPU requests. It does not acknowledge upstream
        // destruction of a GPU generation still retained by Renderer fences.
        Binding = {};
    }
    return Status;
}
FUITextureId FImGuiTextureAdapter::Resolve(uint64 Token) const noexcept
{
    if (bFailed || Token == 0) return {};
    for (const auto& Binding : Bindings)
        if (Binding.Token == Token) return Binding.Id;
    return {};
}
ERHIResult FImGuiTextureAdapter::Fail(const char* Reason) noexcept
{
    bFailed = true;
    Diagnostic = Reason;
    return ERHIResult::Unavailable;
}
ERHIResult FImGuiTextureAdapter::Defer(uint64 Now) noexcept
{
    if (!bPending) { bPending = true; PendingSince = Now; RetryFrames = 0; }
    if (++RetryFrames >= 120) return Fail("ui-texture-retry-frame-limit");
    if (Now - PendingSince >= 5000) return Fail("ui-texture-retry-deadline");
    Diagnostic = "ui-texture-preparation-pending";
    return ERHIResult::NotReady;
}
ERHIResult FImGuiTextureAdapter::Process(std::span<ImTextureData* const> Textures,
    uint64 FrameId, bool bEligible, uint64 NowMilliseconds)
{
    if (bFailed) return ERHIResult::Unavailable;
    LastTime = std::max(LastTime, NowMilliseconds);
    if (!bEligible || FrameId == 0 || FrameId <= LastFrame) return ERHIResult::NotReady;
    LastFrame = FrameId;
    if (bPending && LastTime - PendingSince >= 5000) return Fail("ui-texture-retry-deadline");
    if (Textures.size() > Bindings.size()) return Fail();
    // Reject malformed input before any Renderer call or upstream acknowledgement.
    for (std::size_t I = 0; I < Textures.size(); ++I)
    {
        const auto* Texture = Textures[I];
        if (!Texture) return Fail();
        for (std::size_t J = 0; J < I; ++J) if (Texture == Textures[J]) return Fail();
        switch (Texture->Status)
        {
        case ImTextureStatus_OK:
        case ImTextureStatus_Destroyed:
        case ImTextureStatus_WantDestroy:
            break;
        case ImTextureStatus_WantCreate:
        case ImTextureStatus_WantUpdates:
            if (Texture->Width <= 0 || Texture->Height <= 0 ||
                Texture->Width > 2048 || Texture->Height > 2048 || !Texture->Pixels ||
                !((Texture->Format == ImTextureFormat_Alpha8 && Texture->BytesPerPixel == 1) ||
                  (Texture->Format == ImTextureFormat_RGBA32 && Texture->BytesPerPixel == 4))) return Fail();
            break;
        default: return Fail();
        }
    }
    uint32 Requests = 0;
    uint64 PayloadBytes = 0;
    bool Pending = false;
    try
    {
        for (auto* Texture : Textures)
        {
            if (Texture->Status == ImTextureStatus_Destroyed) continue;
            auto It = std::find_if(Bindings.begin(), Bindings.end(),
                [Texture](const auto& Binding) { return Binding.Source == Texture; });
            if (Texture->Status == ImTextureStatus_OK)
            {
                if (It == Bindings.end() || !It->Id.IsValid() || It->Token != Texture->GetTexID()) return Fail();
                continue;
            }
            if (It == Bindings.end())
            {
                if (Texture->Status != ImTextureStatus_WantCreate) return Fail();
                It = std::find_if(Bindings.begin(), Bindings.end(),
                    [](const auto& Binding) { return !Binding.Source; });
                if (It == Bindings.end()) return Fail();
                It->Source = Texture;
            }
            const bool Destroy = Texture->Status == ImTextureStatus_WantDestroy;
            const bool Create = Texture->Status == ImTextureStatus_WantCreate;
            if ((Create && It->Id.IsValid()) || (!Create && !It->Id.IsValid())) return Fail();
            const uint64 Bytes = Destroy ? 0 : static_cast<uint64>(Texture->Width) * Texture->Height * 4;
            if (Requests >= 64 || Bytes > 16ull * 1024 * 1024 - PayloadBytes)
            { Pending = true; continue; }
            ++Requests; PayloadBytes += Bytes;
            if (LastRequest == std::numeric_limits<uint64>::max() ||
                (!Destroy && LastToken == std::numeric_limits<uint64>::max())) return Fail();
            if (It->PendingRequest == 0) It->PendingRequest = ++LastRequest;
            FUITextureRequest Request;
            Request.RequestId = It->PendingRequest;
            Request.Operation = Destroy ? EUITextureOperation::Destroy
                : Create ? EUITextureOperation::Create : EUITextureOperation::Update;
            Request.LogicalSlot = Create ? static_cast<uint32>(It - Bindings.begin()) + 1 : 0;
            Request.TextureId = It->Id; Request.ExpectedGeneration = It->Id.Generation;
            if (!Destroy)
            {
                Request.Width = static_cast<uint32>(Texture->Width);
                Request.Height = static_cast<uint32>(Texture->Height);
                const bool Colored = Texture->Format == ImTextureFormat_RGBA32 && Texture->UseColors;
                Request.ColorDomain = Colored ? EUITextureColorDomain::SRGBRec709 : EUITextureColorDomain::AlphaCoverage;
                Request.Format = Colored ? ERHIFormat::R8G8B8A8_sRGB : ERHIFormat::R8G8B8A8_UNorm;
                Request.PixelBytes.resize(static_cast<std::size_t>(Bytes));
                for (std::size_t Pixel = 0; Pixel < Bytes / 4; ++Pixel)
                {
                    auto* Destination = Request.PixelBytes.data() + Pixel * 4;
                    if (Texture->Format == ImTextureFormat_Alpha8)
                    {
                        Destination[0] = Destination[1] = Destination[2] = 255;
                        Destination[3] = Texture->Pixels[Pixel];
                    }
                    else std::copy_n(Texture->Pixels + Pixel * 4, 4, Destination);
                }
            }
            const auto Result = Prepare(Request);
            if (!Result.IsValid() || Result.RequestId != Request.RequestId) return Fail();
            if (Result.Result == ERHIResult::NotReady) { Pending = true; continue; }
            if (!Result.Succeeded()) return Fail();
            if (Destroy)
            {
                if (Result.TextureId != It->Id) return Fail();
                if (Result.State != EUITextureState::Destroyed) { Pending = true; continue; }
                Texture->SetTexID(ImTextureID_Invalid);
                Texture->BackendUserData = nullptr;
                Texture->SetStatus(ImTextureStatus_Destroyed);
                *It = {};
            }
            else
            {
                if ((Result.State != EUITextureState::Prepared && Result.State != EUITextureState::Ready &&
                     Result.State != EUITextureState::UploadQueued) ||
                    Result.TextureId.Slot != static_cast<uint32>(It - Bindings.begin()) + 1 ||
                    (!Create && !Result.TextureId.IsNewerThan(It->Id))) return Fail();
                It->Id = Result.TextureId; It->Token = ++LastToken; It->PendingRequest = 0;
                Texture->SetTexID(It->Token);
                Texture->SetStatus(ImTextureStatus_OK);
            }
        }
    }
    catch (const std::bad_alloc&) { return Fail(); }
    if (Pending) return Defer(LastTime);
    bPending = false; RetryFrames = 0; Diagnostic = "";
    return ERHIResult::Success;
}
}
