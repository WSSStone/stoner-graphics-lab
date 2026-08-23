#pragma once

#include "Asset/AssetMinimal.h"
#include "Renderer/FStaticModelRealization.h"
#include "RHI/RHIMinimal.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <span>
#include <string_view>

namespace Stoner::Tests::StaticModelRealization
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;

enum class EFailurePoint
{
    None,
    BufferCreate,
    BufferUpload,
    TextureCreate,
    TextureUpload,
    ShaderCreate,
    LayoutCreate,
    DescriptorCreate,
    DescriptorUpdate,
    SamplerCreate,
    PipelineCreate
};

struct FResourceLedger
{
    EFailurePoint Failure = EFailurePoint::None;
    int FailureOccurrence = 1;
    bool bLoseDeviceAfterFirstCreate = false;
    int CreateSerial = 0;
    std::map<EFailurePoint, int> Calls;
    TArray<FString> Created;
    TArray<FString> Released;
    std::map<std::string, int> ReleaseCounts;

    [[nodiscard]] bool ShouldFail(EFailurePoint Point)
    {
        const int Call = ++Calls[Point];
        return Failure == Point && Call == FailureOccurrence;
    }

    [[nodiscard]] FString CreateId(const char* Kind)
    {
        FString Id(std::string(Kind) + "." +
            std::to_string(++CreateSerial));
        Created.push_back(Id);
        return Id;
    }

    void Release(const FString& Id)
    {
        ++ReleaseCounts[Id.ToStdString()];
        Released.push_back(Id);
    }

    [[nodiscard]] bool EveryCreatedReleasedOnce() const
    {
        if (Created.size() != Released.size()) return false;
        for (const auto& Id : Created)
        {
            const auto Found = ReleaseCounts.find(Id.ToStdString());
            if (Found == ReleaseCounts.end() || Found->second != 1)
                return false;
        }
        return true;
    }
};

class FTrackedBuffer final : public IRHIBuffer
{
public:
    FTrackedBuffer(FRHIBufferDesc Desc, TSharedPtr<FResourceLedger> Ledger)
        : Desc_(Desc), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("buffer")), Data_(Desc.SizeInBytes, 0) {}
    const FRHIBufferDesc& GetDesc() const noexcept override { return Desc_; }
    uint64 GetSizeInBytes() const noexcept override { return Desc_.SizeInBytes; }
    ERHIBufferUsage GetUsage() const noexcept override { return Desc_.Usage; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
    [[nodiscard]] bool Write(const FRHIBufferUploadDesc& Upload)
    {
        if (!Upload.Data || Upload.DataSizeBytes == 0 ||
            Upload.DestinationOffset > Data_.size() ||
            Upload.DataSizeBytes > Data_.size() - Upload.DestinationOffset)
            return false;
        std::memcpy(
            Data_.data() + Upload.DestinationOffset,
            Upload.Data, Upload.DataSizeBytes);
        return true;
    }
    [[nodiscard]] const TArray<uint8>& GetData() const noexcept
    { return Data_; }
private:
    FRHIBufferDesc Desc_;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    TArray<uint8> Data_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedTexture final : public IRHITexture
{
public:
    FTrackedTexture(FRHITextureDesc Desc, TSharedPtr<FResourceLedger> Ledger)
        : Desc_(std::move(Desc)), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("texture")) {}
    const FRHITextureDesc& GetDesc() const noexcept override { return Desc_; }
    ERHITextureDimension GetDimension() const noexcept override
    { return Desc_.Dimension; }
    ERHIFormat GetFormat() const noexcept override { return Desc_.Format; }
    ERHITextureUsage GetUsage() const noexcept override { return Desc_.Usage; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
private:
    FRHITextureDesc Desc_;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedSampler final : public IRHISampler
{
public:
    FTrackedSampler(FRHISamplerDesc Desc, TSharedPtr<FResourceLedger> Ledger)
        : Desc_(Desc), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("sampler")) {}
    const FRHISamplerDesc& GetDesc() const noexcept override { return Desc_; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
private:
    FRHISamplerDesc Desc_;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedShader final : public IRHIShaderModule
{
public:
    FTrackedShader(FRHIShaderModuleDesc Desc, TSharedPtr<FResourceLedger> Ledger)
        : Desc_(std::move(Desc)), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("shader")) {}
    const FRHIShaderModuleDesc& GetDesc() const noexcept override { return Desc_; }
    ERHIShaderStage GetStage() const noexcept override { return Desc_.Stage; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
private:
    FRHIShaderModuleDesc Desc_;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedLayout final : public IRHIPipelineLayout
{
public:
    FTrackedLayout(FRHIPipelineLayoutDesc Desc, TSharedPtr<FResourceLedger> Ledger)
        : Desc_(std::move(Desc)), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("layout")) {}
    const FRHIPipelineLayoutDesc& GetDesc() const noexcept override { return Desc_; }
    uint32 GetSetCount() const noexcept override
    {
        uint32 Count = 0;
        for (const auto& Binding : Desc_.Bindings)
            Count = std::max(Count, Binding.SetIndex + 1);
        return Count;
    }
    const FRHIDescriptorBinding* FindBinding(
        uint32 Set, uint32 Slot) const noexcept override
    {
        const auto Found = std::find_if(
            Desc_.Bindings.begin(), Desc_.Bindings.end(),
            [Set, Slot](const auto& Binding)
            { return Binding.SetIndex == Set && Binding.BindingSlot == Slot; });
        return Found == Desc_.Bindings.end() ? nullptr : &*Found;
    }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
private:
    FRHIPipelineLayoutDesc Desc_;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedDescriptorSet final : public IRHIDescriptorSet
{
public:
    FTrackedDescriptorSet(
        TSharedPtr<IRHIPipelineLayout> Layout, uint32 Set,
        TSharedPtr<FResourceLedger> Ledger)
        : Layout_(std::move(Layout)), Set_(Set), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("descriptor")) {}
    uint32 GetSetIndex() const noexcept override { return Set_; }
    TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept override
    { return Layout_; }
    ERHIDescriptorResourceKind GetBoundResourceKind(
        uint32 Slot, uint32 Element = 0) const noexcept override
    {
        const auto Found = Bound_.find({Slot, Element});
        return Found == Bound_.end()
            ? ERHIDescriptorResourceKind::None : Found->second;
    }
    uint32 GetBoundResourceCount() const noexcept override
    { return static_cast<uint32>(Bound_.size()); }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult UpdateBuffer(uint32 Slot, uint32 Element,
        const TSharedPtr<IRHIBuffer>& Value) override
    { return Bind(Slot, Element, Value != nullptr, ERHIDescriptorResourceKind::Buffer); }
    ERHIResult UpdateTexture(uint32 Slot, uint32 Element,
        const TSharedPtr<IRHITexture>& Value) override
    { return Bind(Slot, Element, Value != nullptr, ERHIDescriptorResourceKind::Texture); }
    ERHIResult UpdateSampler(uint32 Slot, uint32 Element,
        const TSharedPtr<IRHISampler>& Value) override
    { return Bind(Slot, Element, Value != nullptr, ERHIDescriptorResourceKind::Sampler); }
    ERHIResult UpdateCombinedTextureSampler(uint32 Slot, uint32 Element,
        const TSharedPtr<IRHITexture>& Texture,
        const TSharedPtr<IRHISampler>& Sampler) override
    { return Bind(Slot, Element, Texture && Sampler,
        ERHIDescriptorResourceKind::CombinedTextureSampler); }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
private:
    ERHIResult Bind(uint32 Slot, uint32 Element, bool bHasResource,
        ERHIDescriptorResourceKind Kind)
    {
        if (!bHasResource || State_ != ERHIResourceLifecycleState::Valid ||
            !Layout_ || !Layout_->FindBinding(Set_, Slot) ||
            Ledger_->ShouldFail(EFailurePoint::DescriptorUpdate))
            return ERHIResult::Failed;
        Bound_[{Slot, Element}] = Kind;
        return ERHIResult::Success;
    }
    TSharedPtr<IRHIPipelineLayout> Layout_;
    uint32 Set_ = 0;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    std::map<std::pair<uint32, uint32>, ERHIDescriptorResourceKind> Bound_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedPipeline final : public IRHIGraphicsPipeline
{
public:
    FTrackedPipeline(
        FRHIGraphicsPipelineDesc Desc, TSharedPtr<FResourceLedger> Ledger)
        : Desc_(std::move(Desc)), Ledger_(std::move(Ledger)),
          Id_(Ledger_->CreateId("pipeline")) {}
    const FRHIGraphicsPipelineDesc& GetDesc() const noexcept override
    { return Desc_; }
    TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept override
    { return Desc_.PipelineLayout; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        if (State_ == ERHIResourceLifecycleState::Invalidated)
            return ERHIResult::InvalidState;
        State_ = ERHIResourceLifecycleState::Invalidated;
        Ledger_->Release(Id_);
        return ERHIResult::Success;
    }
private:
    FRHIGraphicsPipelineDesc Desc_;
    TSharedPtr<FResourceLedger> Ledger_;
    FString Id_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedRenderPass final : public IRHIRenderPass
{
public:
    explicit FTrackedRenderPass(FRHIRenderPassDesc Desc)
        : Desc_(std::move(Desc)) {}
    const FRHIRenderPassDesc& GetDesc() const noexcept override { return Desc_; }
    uint32 GetAttachmentCount() const noexcept override
    { return static_cast<uint32>(Desc_.Attachments.size()); }
    const FRHIRenderPassAttachmentDesc* GetAttachment(
        uint32 Index) const noexcept override
    { return Index < Desc_.Attachments.size() ? &Desc_.Attachments[Index] : nullptr; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        State_ = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }
private:
    FRHIRenderPassDesc Desc_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedFramebuffer final : public IRHIFramebuffer
{
public:
    explicit FTrackedFramebuffer(FRHIFramebufferDesc Desc)
        : Desc_(std::move(Desc)) {}
    const FRHIFramebufferDesc& GetDesc() const noexcept override { return Desc_; }
    TSharedPtr<IRHIRenderPass> GetRenderPass() const noexcept override
    { return Desc_.RenderPass; }
    uint32 GetWidth() const noexcept override { return Desc_.Width; }
    uint32 GetHeight() const noexcept override { return Desc_.Height; }
    uint32 GetAttachmentCount() const noexcept override
    { return static_cast<uint32>(Desc_.Attachments.size()); }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    { return State_; }
    ERHIResult Invalidate() override
    {
        State_ = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }
private:
    FRHIFramebufferDesc Desc_;
    ERHIResourceLifecycleState State_ = ERHIResourceLifecycleState::Valid;
};

class FTrackedCommandBuffer final : public IRHICommandBuffer
{
public:
    ERHICommandBufferState GetState() const noexcept override { return State_; }
    ERHIQueueType GetCompatibleQueueType() const noexcept override
    { return ERHIQueueType::Graphics; }
    uint32 GetRecordedCommandCount() const noexcept override { return Count_; }
    ERHIResult Begin() override
    {
        if (State_ != ERHICommandBufferState::Idle &&
            State_ != ERHICommandBufferState::Resettable)
            return ERHIResult::InvalidState;
        State_ = ERHICommandBufferState::Recording;
        Count_ = 0;
        bInRenderPass_ = false;
        return ERHIResult::Success;
    }
    ERHIResult End() override
    {
        if (State_ != ERHICommandBufferState::Recording || bInRenderPass_)
            return ERHIResult::InvalidState;
        State_ = ERHICommandBufferState::Completed;
        return ERHIResult::Success;
    }
    ERHIResult Reset() override
    {
        State_ = ERHICommandBufferState::Idle;
        Count_ = 0;
        bInRenderPass_ = false;
        return ERHIResult::Success;
    }
    ERHIResult RecordDraw(uint32 Vertices, uint32 Instances) override
    { return Record(Vertices > 0 && Instances > 0 && bInRenderPass_); }
    ERHIResult RecordDrawIndexed(
        uint32 Indices, uint32 Instances, uint32) override
    { return Record(Indices > 0 && Instances > 0 && bInRenderPass_); }
    ERHIResult RecordDispatch(uint32, uint32, uint32) override
    { return ERHIResult::Unsupported; }
    ERHIResult BindGraphicsPipeline(
        const TSharedPtr<IRHIGraphicsPipeline>& Value) override
    { return Record(Value && bInRenderPass_); }
    ERHIResult BindComputePipeline(
        const TSharedPtr<IRHIComputePipeline>&) override
    { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier() override { return Record(!bInRenderPass_); }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override
    { return Record(!bInRenderPass_); }
    ERHIResult RecordBufferCopy(
        const TSharedPtr<IRHIBuffer>&, const TSharedPtr<IRHIBuffer>&,
        FRHIBufferCopyRange) override
    { return Record(!bInRenderPass_); }
    ERHIResult RecordTextureCopy(
        const TSharedPtr<IRHITexture>&, const TSharedPtr<IRHITexture>&,
        FRHITextureCopyRegion) override
    { return Record(!bInRenderPass_); }
    ERHIResult RecordLayoutTransition(
        const FRHIResourceBarrierDesc& Value) override
    { return Record(!bInRenderPass_ && (Value.Texture || Value.Buffer)); }
    ERHIResult BeginRenderPass(
        const TSharedPtr<IRHIRenderPass>& Pass,
        const TSharedPtr<IRHIFramebuffer>& Framebuffer) override
    {
        if (!Pass || !Framebuffer || Framebuffer->GetRenderPass() != Pass ||
            State_ != ERHICommandBufferState::Recording || bInRenderPass_)
            return ERHIResult::InvalidState;
        bInRenderPass_ = true;
        ++Count_;
        return ERHIResult::Success;
    }
    ERHIResult BeginRenderPass(
        const TSharedPtr<IRHIRenderPass>& Pass,
        const TSharedPtr<IRHIFramebuffer>& Framebuffer,
        const FRHIRenderPassClearValues&) override
    {
        return BeginRenderPass(Pass, Framebuffer);
    }
    ERHIResult EndRenderPass() override
    {
        if (!bInRenderPass_) return ERHIResult::InvalidState;
        bInRenderPass_ = false;
        ++Count_;
        return ERHIResult::Success;
    }
    ERHIResult BindVertexBuffer(
        const TSharedPtr<IRHIBuffer>& Value, uint64) override
    { return Record(Value && bInRenderPass_); }
    ERHIResult BindIndexBuffer(
        const TSharedPtr<IRHIBuffer>& Value, ERHIIndexType, uint64) override
    { return Record(Value && bInRenderPass_); }
    ERHIResult BindDescriptorSet(
        const TSharedPtr<IRHIDescriptorSet>& Value) override
    { return Record(Value && bInRenderPass_); }
    ERHIResult RecordTextureToBufferCopy(
        const TSharedPtr<IRHITexture>& Source,
        const TSharedPtr<IRHIBuffer>& Destination,
        FRHITextureBufferCopyRegion) override
    { return Record(Source && Destination && !bInRenderPass_); }
    ERHIResult SetViewport(const FRHIViewport&) override
    { return Record(bInRenderPass_); }
    ERHIResult SetScissor(const FRHIScissorRect&) override
    { return Record(bInRenderPass_); }
private:
    ERHIResult Record(bool bValid)
    {
        if (!bValid || State_ != ERHICommandBufferState::Recording)
            return ERHIResult::InvalidState;
        ++Count_;
        return ERHIResult::Success;
    }
    ERHICommandBufferState State_ = ERHICommandBufferState::Idle;
    uint32 Count_ = 0;
    bool bInRenderPass_ = false;
};

class FDevice final : public IRHIDevice
{
public:
    explicit FDevice(TSharedPtr<FResourceLedger> Ledger = MakeShared<FResourceLedger>())
        : Ledger_(std::move(Ledger))
    {
        Capabilities_.bSupportsGraphicsQueue = true;
        Capabilities_.MaxInFlightFrames = 2;
        Capabilities_.MaxCommandBuffersPerQueue = 16;
        Capabilities_.MaxQueuesPerType = 1;
        Capabilities_.MaxBufferSizeBytes = 1ULL << 30U;
        Capabilities_.MaxResourceSizeBytes = 1ULL << 30U;
        Capabilities_.MaxTextureDimension1D = 8192;
        Capabilities_.MaxTextureDimension2D = 8192;
        Capabilities_.MaxTextureDimension3D = 2048;
        Capabilities_.MaxTextureArrayLayers = 256;
        Capabilities_.MaxPerStageBufferBindings = 16;
        Capabilities_.MaxPerStageTextureBindings = 16;
        Capabilities_.MaxPerStageSamplerBindings = 16;
        Capabilities_.MaxConstantRangeBytes = 128;
        Capabilities_.MaxConstantDataBytesPerStage = 128;
        Capabilities_.SupportedSampleCounts = static_cast<uint32>(ERHISampleCount::One);
        Capabilities_.Formats = {
            MakeRHIFormatCapabilities(ERHIFormat::R8_UNorm),
            MakeRHIFormatCapabilities(ERHIFormat::R8G8_UNorm),
            MakeRHIFormatCapabilities(ERHIFormat::R8G8B8A8_UNorm),
            MakeRHIFormatCapabilities(ERHIFormat::R8G8B8A8_sRGB),
            MakeRHIFormatCapabilities(ERHIFormat::R16G16B16A16_Float),
            MakeRHIFormatCapabilities(ERHIFormat::D32_Float)};
    }
    ERHIDeviceState GetState() const noexcept override
    { return bActive_ ? ERHIDeviceState::Active : ERHIDeviceState::Shutdown; }
    const FRHIDeviceCapabilities& GetCapabilities() const noexcept override
    { return Capabilities_; }
    bool IsActive() const noexcept override { return bActive_; }
    ERHIResult Shutdown() override { bActive_ = false; return ERHIResult::Success; }

    TRHIObjectResult<IRHIBuffer> CreateBuffer(const FRHIBufferDesc& Desc) override
    {
        if (!bActive_ || Ledger_->ShouldFail(EFailurePoint::BufferCreate))
            return {ERHIResult::Failed, nullptr};
        auto Result = MakeShared<FTrackedBuffer>(Desc, Ledger_);
        if (Ledger_->bLoseDeviceAfterFirstCreate && Ledger_->Created.size() == 1)
            bActive_ = false;
        return {ERHIResult::Success, Result};
    }
    ERHIResult UploadBuffer(const TSharedPtr<IRHIBuffer>& Buffer,
        const FRHIBufferUploadDesc& Upload) override
    {
        if (!bActive_ || !Buffer || !Upload.Data || Upload.DataSizeBytes == 0 ||
            Ledger_->ShouldFail(EFailurePoint::BufferUpload))
            return ERHIResult::Failed;
        const auto Tracked = std::dynamic_pointer_cast<FTrackedBuffer>(Buffer);
        return Tracked && Tracked->Write(Upload)
            ? ERHIResult::Success : ERHIResult::Failed;
    }
    TRHIObjectResult<IRHITexture> CreateTexture(const FRHITextureDesc& Desc) override
    {
        if (!bActive_ || Ledger_->ShouldFail(EFailurePoint::TextureCreate))
            return {ERHIResult::Failed, nullptr};
        return {ERHIResult::Success,
            MakeShared<FTrackedTexture>(Desc, Ledger_)};
    }
    ERHIResult UploadTexture(const TSharedPtr<IRHITexture>& Texture,
        const FRHITextureUploadDesc& Upload) override
    {
        if (!bActive_ || !Texture || !Upload.Data ||
            Ledger_->ShouldFail(EFailurePoint::TextureUpload))
            return ERHIResult::Failed;
        return ERHIResult::Success;
    }
    TRHIObjectResult<IRHISampler> CreateSampler(const FRHISamplerDesc& Desc) override
    {
        if (!bActive_ || Ledger_->ShouldFail(EFailurePoint::SamplerCreate))
            return {ERHIResult::Failed, nullptr};
        return {ERHIResult::Success, MakeShared<FTrackedSampler>(Desc, Ledger_)};
    }
    TRHIObjectResult<IRHIShaderModule> CreateShaderModule(
        const FRHIShaderModuleDesc& Desc) override
    {
        if (!bActive_ || Ledger_->ShouldFail(EFailurePoint::ShaderCreate))
            return {ERHIResult::Failed, nullptr};
        return {ERHIResult::Success, MakeShared<FTrackedShader>(Desc, Ledger_)};
    }
    TRHIObjectResult<IRHIPipelineLayout> CreatePipelineLayout(
        const FRHIPipelineLayoutDesc& Desc) override
    {
        if (!bActive_ || Ledger_->ShouldFail(EFailurePoint::LayoutCreate))
            return {ERHIResult::Failed, nullptr};
        return {ERHIResult::Success, MakeShared<FTrackedLayout>(Desc, Ledger_)};
    }
    TRHIObjectResult<IRHIDescriptorSet> CreateDescriptorSet(
        const TSharedPtr<IRHIPipelineLayout>& Layout, uint32 Set) override
    {
        if (!bActive_ || !Layout ||
            Ledger_->ShouldFail(EFailurePoint::DescriptorCreate))
            return {ERHIResult::Failed, nullptr};
        return {ERHIResult::Success,
            MakeShared<FTrackedDescriptorSet>(Layout, Set, Ledger_)};
    }
    TRHIObjectResult<IRHIGraphicsPipeline> CreateGraphicsPipeline(
        const FRHIGraphicsPipelineDesc& Desc) override
    {
        if (!bActive_ || Ledger_->ShouldFail(EFailurePoint::PipelineCreate))
            return {ERHIResult::Failed, nullptr};
        return {ERHIResult::Success,
            MakeShared<FTrackedPipeline>(Desc, Ledger_)};
    }

#define SG028_UNSUPPORTED(Name, Type, Arguments) \
    TRHIObjectResult<Type> Name Arguments override \
    { return {ERHIResult::Unsupported, nullptr}; }
    SG028_UNSUPPORTED(CreateCommandQueue, IRHICommandQueue, (ERHIQueueType))
    TRHIObjectResult<IRHICommandBuffer> CreateCommandBuffer(
        ERHIQueueType Type) override
    {
        return Type == ERHIQueueType::Graphics
            ? TRHIObjectResult<IRHICommandBuffer>{
                ERHIResult::Success, MakeShared<FTrackedCommandBuffer>()}
            : TRHIObjectResult<IRHICommandBuffer>{
                ERHIResult::Unsupported, nullptr};
    }
    SG028_UNSUPPORTED(CreateFence, IRHIFence, (bool))
    SG028_UNSUPPORTED(CreateSemaphore, IRHISemaphore, ())
    SG028_UNSUPPORTED(CreateSwapchain, IRHISwapchain, (uint32))
    SG028_UNSUPPORTED(CreateComputePipeline, IRHIComputePipeline,
        (const FRHIComputePipelineDesc&))
    TRHIObjectResult<IRHIRenderPass> CreateRenderPass(
        const FRHIRenderPassDesc& Desc) override
    {
        return IsValidRHIRenderPassDesc(Desc)
            ? TRHIObjectResult<IRHIRenderPass>{
                ERHIResult::Success, MakeShared<FTrackedRenderPass>(Desc)}
            : TRHIObjectResult<IRHIRenderPass>{
                ERHIResult::InvalidState, nullptr};
    }
    TRHIObjectResult<IRHIFramebuffer> CreateFramebuffer(
        const FRHIFramebufferDesc& Desc) override
    {
        const bool bValid = Desc.RenderPass && Desc.Width > 0 && Desc.Height > 0 &&
            Desc.Attachments.size() == Desc.RenderPass->GetAttachmentCount();
        return bValid
            ? TRHIObjectResult<IRHIFramebuffer>{
                ERHIResult::Success, MakeShared<FTrackedFramebuffer>(Desc)}
            : TRHIObjectResult<IRHIFramebuffer>{
                ERHIResult::InvalidState, nullptr};
    }
#undef SG028_UNSUPPORTED

    [[nodiscard]] const TSharedPtr<FResourceLedger>& Ledger() const noexcept
    { return Ledger_; }
    void RemoveFormat(ERHIFormat Format)
    {
        std::erase_if(
            Capabilities_.Formats,
            [Format](const auto& Record) { return Record.Format == Format; });
    }
private:
    TSharedPtr<FResourceLedger> Ledger_;
    FRHIDeviceCapabilities Capabilities_;
    bool bActive_ = true;
};

inline FAssetId Id(const char* Type, const char* Path,
    const char* Subresource = nullptr)
{
    FAssetId Result;
    std::optional<FString> Sub;
    if (Subresource) Sub = FString(Subresource);
    (void)FAssetId::Create(FString(Type), FString(Path), Sub, Result);
    return Result;
}

inline FAssetVersion Version(std::string_view Text)
{
    FAssetVersion Result;
    const auto Bytes = std::span<const uint8>(
        reinterpret_cast<const uint8*>(Text.data()), Text.size());
    Result.SourceDigest = FAssetDigest::FromBytes(Bytes);
    Result.ContentDigest = Result.SourceDigest;
    return Result;
}

inline TArray<uint8> Spirv(EShaderStage Stage)
{
    const uint32 Model = Stage == EShaderStage::Vertex ? 0U : 4U;
    const uint32 Words[] = {
        0x07230203U, 0x00010000U, 0U, 2U, 0U,
        (5U << 16U) | 15U, Model, 1U, 0x6e69616dU, 0U};
    TArray<uint8> Bytes(sizeof(Words));
    std::memcpy(Bytes.data(), Words, sizeof(Words));
    return Bytes;
}

inline TSharedPtr<const FKTX2TextureArtifact> OpenTexture(
    const char* Name = "uncompressed-rgba8-alpha.ktx2")
{
    std::ifstream Input(
        std::string("Tests/Fixtures/KTX2/Golden/") + Name,
        std::ios::binary);
    TArray<uint8> Bytes{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
    FTextureCookLimits Limits;
    FKTX2TextureInfo Info;
    if (FKTX2TextureCodec::Inspect(Bytes, Limits, Info, nullptr) !=
        EAssetResult::Success) return {};
    FKTX2TextureArtifact Artifact;
    if (FKTX2TextureCodec::Open(
            Info.TextureId, Bytes, Limits, Artifact, nullptr) !=
        EAssetResult::Success) return {};
    return MakeShared<const FKTX2TextureArtifact>(std::move(Artifact));
}

struct FFixture
{
    FStaticModelRealizationRequest Request;
};

inline FFixture MakeFixture(
    bool bIncludeUnusedUntexturedMaterial = false,
    bool bUsePerTextureProfiles = false)
{
    FFixture Fixture;
    const auto Texture = OpenTexture();
    const auto DataTexture = bUsePerTextureProfiles
        ? OpenTexture("uncompressed-rgb8-srgb.ktx2") : Texture;
    const FAssetId ShaderId = Id(
        "ShaderProgram", "Tests/Renderer/StaticModel/Surface");
    const FAssetId MaterialA = Id(
        "Material", "Tests/Renderer/StaticModel/MaterialA");
    const FAssetId MaterialB = Id(
        "Material", "Tests/Renderer/StaticModel/MaterialB");
    const FAssetId MeshId = Id(
        "StaticMesh", "Tests/Renderer/StaticModel/Mesh", "mesh.0");

    TArray<TSharedPtr<const FShaderPayloadAsset>> Payloads;
    FShaderAssetDesc ShaderDesc;
    ShaderDesc.Id = ShaderId;
    ShaderDesc.Version = Version("shader");
    for (const auto Stage : {EShaderStage::Vertex, EShaderStage::Fragment})
    {
        const char* Suffix = Stage == EShaderStage::Vertex ? "vertex" : "fragment";
        const auto Bytes = Spirv(Stage);
        FShaderPayloadAsset Payload;
        const FAssetId PayloadId = Id(
            "ShaderPayload", "Tests/Renderer/StaticModel/Surface", Suffix);
        FAssetVersion PayloadVersion = Version(Suffix);
        PayloadVersion.SourceDigest = FAssetDigest::FromBytes(Bytes);
        PayloadVersion.ContentDigest = PayloadVersion.SourceDigest;
        (void)FShaderPayloadAsset::Create(
            PayloadId, PayloadVersion, EShaderBackendFamily::Vulkan,
            FString("vulkan-1.3"), EShaderPayloadFormat::SPIRV, Stage,
            FString("main"), {}, Bytes, Payload);
        auto SharedPayload = MakeShared<const FShaderPayloadAsset>(
            std::move(Payload));
        Payloads.push_back(SharedPayload);

        FShaderSourceReference Source;
        Source.Stage = Stage;
        Source.EntryPoint = FString("main");
        const FAssetId SourceId = Id(
            "ShaderSource", "Tests/Renderer/StaticModel/Surface", Suffix);
        (void)TSoftAssetRef<FShaderSourceAsset>::Create(SourceId, Source.Source);
        Source.Locator = FString(std::string(Suffix) + ".glsl");
        Source.ExpectedDigest = Version(Suffix).ContentDigest;
        ShaderDesc.Stages.push_back(std::move(Source));

        FShaderPayloadReference Reference;
        Reference.Backend = EShaderBackendFamily::Vulkan;
        Reference.Profile = FString("vulkan-1.3");
        Reference.Format = EShaderPayloadFormat::SPIRV;
        Reference.Stage = Stage;
        Reference.EntryPoint = FString("main");
        (void)TSoftAssetRef<FShaderPayloadAsset>::Create(
            PayloadId, Reference.Payload);
        Reference.Locator = FString(std::string(Suffix) + ".spv");
        Reference.ExpectedDigest = FAssetDigest::FromBytes(Bytes);
        Reference.Producer = FString("Stoner.Tests");
        Reference.ProducerVersion = FString("028-v1");
        if (ShaderDesc.Variants.empty())
        {
            FShaderVariantDefinition Variant;
            Variant.VariantName = FString("default");
            ShaderDesc.Variants.push_back(std::move(Variant));
        }
        ShaderDesc.Variants.front().Payloads.push_back(std::move(Reference));
    }
    ShaderDesc.RequiredParameters.push_back({
        FString("BaseColorTexture"),
        EMaterialAssetParameterType::TextureBinding});
    ShaderDesc.InterfaceBindings = {
        {0, 0, EShaderResourceKind::UniformBuffer, 1,
            {EShaderStage::Vertex}, FString("Frame")},
        {1, 0, EShaderResourceKind::UniformBuffer, 1,
            {EShaderStage::Vertex, EShaderStage::Fragment},
            FString("DrawMaterial")},
        {1, 1, EShaderResourceKind::CombinedTextureSampler, 1,
            {EShaderStage::Fragment}, FString("BaseColorTexture")},
        {1, 2, EShaderResourceKind::CombinedTextureSampler, 1,
            {EShaderStage::Fragment}, FString("MetallicRoughnessTexture")},
        {1, 3, EShaderResourceKind::CombinedTextureSampler, 1,
            {EShaderStage::Fragment}, FString("NormalTexture")},
        {1, 4, EShaderResourceKind::CombinedTextureSampler, 1,
            {EShaderStage::Fragment}, FString("OcclusionTexture")},
        {1, 5, EShaderResourceKind::CombinedTextureSampler, 1,
            {EShaderStage::Fragment}, FString("EmissiveTexture")}};
    FShaderAsset Shader;
    (void)FShaderAsset::CreateValidated(std::move(ShaderDesc), Shader);

    TArray<TSharedPtr<const FMaterialAsset>> Materials;
    for (const FAssetId& MaterialId : {MaterialA, MaterialB})
    {
        FMaterialAssetDesc Desc;
        Desc.Id = MaterialId;
        Desc.Version = Version(MaterialId == MaterialA ? "material-a" : "material-b");
        Desc.SchemaVersion = 2;
        (void)TSoftAssetRef<FShaderAsset>::Create(ShaderId, Desc.Shader);
        FMaterialTextureBinding Binding;
        const auto MaterialTexture = MaterialId == MaterialB
            ? DataTexture : Texture;
        if (MaterialTexture &&
            !(bIncludeUnusedUntexturedMaterial && MaterialId == MaterialB))
        {
            (void)FMaterialTextureBinding::Create(
                MaterialTexture->GetId(), 0, {}, Binding);
            Desc.Parameters.push_back({FString("BaseColorTexture"),
                FMaterialAssetParameterValue::FromTextureBinding(Binding)});
        }
        FMaterialAsset Material;
        (void)FMaterialAsset::CreateValidated(std::move(Desc), Material);
        Materials.push_back(MakeShared<const FMaterialAsset>(std::move(Material)));
    }

    FStaticMeshAssetDesc MeshDesc;
    MeshDesc.Id = MeshId;
    MeshDesc.Version = Version("mesh");
    MeshDesc.ImportProfileDigest = Version("profile").ContentDigest;
    MeshDesc.Bounds.Box = FBox(FVector3(0, 0, 0), FVector3(3, 1, 0));
    MeshDesc.Bounds.Sphere = FSphere(FVector3(1.5f, 0.5f, 0), 2.0f);
    for (uint32 Index = 0; Index < 2; ++Index)
    {
        FStaticMeshMaterialSlot Slot;
        Slot.StableKey = FString("material." + std::to_string(Index));
        const FAssetId& MaterialId = Index == 0 ? MaterialA : MaterialB;
        (void)TSoftAssetRef<FMaterialAsset>::Create(MaterialId, Slot.Material);
        MeshDesc.MaterialSlots.push_back(std::move(Slot));
        MeshDesc.Dependencies.push_back({MaterialId,
            EAssetDependencyRole::Runtime, EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Unresolved});

        FStaticMeshPrimitive Primitive;
        Primitive.StableKey = FString("primitive." + std::to_string(Index));
        Primitive.SourcePrimitiveIndex = Index;
        Primitive.MaterialSlotIndex = bIncludeUnusedUntexturedMaterial ? 0 : Index;
        const float X = static_cast<float>(Index) * 2.0f;
        Primitive.Vertices.Positions = {
            FVector3(X, 0, 0), FVector3(X + 1, 0, 0), FVector3(X, 1, 0)};
        Primitive.Vertices.Normals = {
            FVector3::UnitZ(), FVector3::UnitZ(), FVector3::UnitZ()};
        Primitive.Vertices.Tangents = {
            FVector4(1, 0, 0, 1), FVector4(1, 0, 0, 1), FVector4(1, 0, 0, 1)};
        Primitive.Vertices.TexCoords[0] = {
            FVector2(0, 0), FVector2(1, 0), FVector2(0, 1)};
        (void)FStaticMeshIndexData::Create({0, 1, 2}, Primitive.Indices);
        Primitive.LocalBounds.Box = FBox(
            FVector3(X, 0, 0), FVector3(X + 1, 1, 0));
        Primitive.LocalBounds.Sphere = FSphere(FVector3(X + 0.5f, 0.5f, 0), 1.0f);
        MeshDesc.Primitives.push_back(std::move(Primitive));
    }
    MeshDesc.SourceManifest.push_back({
        Id("StaticMeshSource", "Tests/Renderer/StaticModel/Mesh"),
        Version("mesh-source"), EAssetSourceRole::Source});
    FStaticMeshAsset Mesh;
    (void)FStaticMeshAsset::CreateValidated(std::move(MeshDesc), Mesh);
    auto SharedMesh = MakeShared<const FStaticMeshAsset>(std::move(Mesh));

    FStaticModelAssetDesc ModelDesc;
    ModelDesc.Id = Id("StaticModel", "Tests/Renderer/StaticModel/Model");
    ModelDesc.Version = Version("model");
    ModelDesc.SceneStableKey = FString("scene.0");
    ModelDesc.RootNodeIndices = {0};
    ModelDesc.Bounds = SharedMesh->GetDesc().Bounds;
    ModelDesc.ImportProfileDigest = Version("profile").ContentDigest;
    for (uint32 Index = 0; Index < 2; ++Index)
    {
        FStaticModelNode Node;
        Node.StableKey = FString("node." + std::to_string(Index));
        Node.DisplayName = Node.StableKey;
        Node.SourceNodeIndex = Index;
        if (Index == 0) Node.Children = {1};
        Node.Mesh.emplace();
        (void)TSoftAssetRef<FStaticMeshAsset>::Create(MeshId, *Node.Mesh);
        ModelDesc.Nodes.push_back(std::move(Node));
    }
    ModelDesc.Dependencies.push_back({MeshId,
        EAssetDependencyRole::Runtime, EAssetDependencyStrength::Required,
        EAssetDependencyResolution::Unresolved});
    ModelDesc.SourceManifest.push_back({
        Id("StaticModelSource", "Tests/Renderer/StaticModel/Model"),
        Version("model-source"), EAssetSourceRole::Source});
    FStaticModelAsset Model;
    (void)FStaticModelAsset::CreateValidated(std::move(ModelDesc), 16, Model);

    std::ifstream ProfileInput(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json",
        std::ios::binary);
    TArray<uint8> ProfileBytes{
        std::istreambuf_iterator<char>(ProfileInput),
        std::istreambuf_iterator<char>()};
    FAssetTargetProfileEvidence Evidence;
    (void)FAssetCookContractCodec::ParseTargetProfile(ProfileBytes, Evidence);

    Fixture.Request.Device = MakeShared<FDevice>();
    Fixture.Request.Model = MakeShared<const FStaticModelAsset>(std::move(Model));
    Fixture.Request.Dependencies.Meshes = {SharedMesh};
    Fixture.Request.Dependencies.Materials = std::move(Materials);
    Fixture.Request.Dependencies.Shaders = {
        MakeShared<const FShaderAsset>(std::move(Shader))};
    Fixture.Request.Dependencies.ShaderPayloads = std::move(Payloads);
    Fixture.Request.Dependencies.Textures = {Texture};
    if (bUsePerTextureProfiles && DataTexture &&
        DataTexture->GetId() != Texture->GetId())
        Fixture.Request.Dependencies.Textures.push_back(DataTexture);
    const auto AddVersion = [&Fixture](
        const FAssetId& AssetId, const FAssetVersion& AssetVersion)
    {
        Fixture.Request.Dependencies.Versions.push_back(
            {AssetId, AssetVersion});
    };
    AddVersion(SharedMesh->GetDesc().Id, SharedMesh->GetDesc().Version);
    for (const auto& Material : Fixture.Request.Dependencies.Materials)
        AddVersion(Material->GetDesc().Id, Material->GetDesc().Version);
    AddVersion(
        Fixture.Request.Dependencies.Shaders.front()->GetDesc().Id,
        Fixture.Request.Dependencies.Shaders.front()->GetDesc().Version);
    for (const auto& Payload : Fixture.Request.Dependencies.ShaderPayloads)
        AddVersion(Payload->GetId(), Payload->GetVersion());
    if (Texture)
    {
        FAssetVersion TextureVersion;
        TextureVersion.SourceDigest = Texture->GetInfo().SourceDigest;
        TextureVersion.ContentDigest = Texture->GetInfo().ContentDigest;
        AddVersion(Texture->GetId(), TextureVersion);
    }
    if (bUsePerTextureProfiles && DataTexture &&
        DataTexture->GetId() != Texture->GetId())
    {
        FAssetVersion TextureVersion;
        TextureVersion.SourceDigest = DataTexture->GetInfo().SourceDigest;
        TextureVersion.ContentDigest = DataTexture->GetInfo().ContentDigest;
        AddVersion(DataTexture->GetId(), TextureVersion);
    }
    Fixture.Request.TargetEvidence =
        MakeShared<const FAssetTargetProfileEvidence>(std::move(Evidence));
    if (bUsePerTextureProfiles)
    {
        for (const auto& TargetTexture :
             Fixture.Request.Dependencies.Textures)
            Fixture.Request.TextureTargetProfiles.push_back({
                TargetTexture->GetId(),
                FTextureTargetProfile::DesktopDefault(
                    TargetTexture->GetInfo())});
    }
    else if (Texture)
        Fixture.Request.TextureTargetProfile =
            FTextureTargetProfile::DesktopDefault(Texture->GetInfo());
    Fixture.Request.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    Fixture.Request.RenderTargets.DepthStencilFormat = ERHIFormat::D32_Float;
    Fixture.Request.RenderTargets.SampleCount = ERHISampleCount::One;
    return Fixture;
}

} // namespace Stoner::Tests::StaticModelRealization
