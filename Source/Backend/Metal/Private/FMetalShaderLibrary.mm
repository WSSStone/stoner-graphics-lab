#include "FMetalShaderLibrary.h"

#include <dispatch/dispatch.h>

#include <new>
#include <string_view>

namespace Stoner::Backend::Metal::Private
{
namespace
{

Core::FString MakeNativeEntryPoint(
    const Core::FString& LogicalEntryPoint)
{
    return LogicalEntryPoint.IsEmpty()
        ? Core::FString()
        : Core::FString(
              "stoner_" + LogicalEntryPoint.ToStdString());
}

bool MatchesFunctionType(
    MTLFunctionType Type,
    RHI::ERHIShaderStage Stage) noexcept
{
    switch (Stage)
    {
    case RHI::ERHIShaderStage::Vertex: return Type == MTLFunctionTypeVertex;
    case RHI::ERHIShaderStage::Fragment: return Type == MTLFunctionTypeFragment;
    case RHI::ERHIShaderStage::Compute: return Type == MTLFunctionTypeKernel;
    default: return false;
    }
}

bool IsTargetCompatible(const Core::FString& Profile) noexcept
{
    const std::string_view Value = Profile.View();
    if (!Value.starts_with("metal-macos-12-")) return false;
#if defined(__aarch64__) || defined(__arm64__)
    return Value.ends_with("arm64");
#elif defined(__x86_64__)
    return Value.ends_with("x86_64");
#else
    return false;
#endif
}

} // namespace

FMetalShaderLibrary::FMetalShaderLibrary(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIShaderModuleDesc Desc,
    id<MTLLibrary> Library,
    id<MTLFunction> Function) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Resource),
      Desc_(std::move(Desc)),
      Library_(Library), Function_(Function)
{
}

FMetalShaderLibrary::~FMetalShaderLibrary()
{
    (void)Invalidate();
    Function_ = nil;
    Library_ = nil;
}
const RHI::FRHIShaderModuleDesc& FMetalShaderLibrary::GetDesc()
    const noexcept { return Desc_; }
RHI::ERHIShaderStage FMetalShaderLibrary::GetStage() const noexcept
{
    return Desc_.Stage;
}
RHI::ERHIResourceLifecycleState FMetalShaderLibrary::GetLifecycleState()
    const noexcept { return GetLifecycle(); }
RHI::ERHIResult FMetalShaderLibrary::Invalidate()
{
    return InvalidateObject();
}
id<MTLFunction> FMetalShaderLibrary::GetNativeFunction() const noexcept
{
    return Function_;
}

RHI::TRHIObjectResult<RHI::IRHIShaderModule> CreateMetalShaderLibrary(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    void* NativeDevice,
    const RHI::FRHIShaderModuleDesc& Desc) noexcept
{
    if (!Owner || NativeDevice == nullptr ||
        !RHI::IsValidRHIShaderModuleDesc(Desc) ||
        Desc.Payload.Format != RHI::ERHIShaderPayloadFormat::MetalLibrary ||
        Desc.RuntimeMode != RHI::ERHIRuntimeObjectMode::RealRuntime ||
        !IsTargetCompatible(Desc.Payload.TargetProfile) ||
        Desc.NativeBindingMap.PolicyVersion !=
            Core::FString("metal-direct-binding-v1"))
        return {RHI::ERHIResult::InvalidState, nullptr};
    @autoreleasepool
    {
        id<MTLDevice> Device = (__bridge id<MTLDevice>)NativeDevice;
        dispatch_data_t Data = dispatch_data_create(
            Desc.Payload.Bytes.data(), Desc.Payload.Bytes.size(),
            dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
            DISPATCH_DATA_DESTRUCTOR_DEFAULT);
        NSError* Error = nil;
        id<MTLLibrary> Library = [Device newLibraryWithData:Data error:&Error];
        if (Library == nil)
            return {RHI::ERHIResult::Failed, nullptr};
        const Core::FString NativeEntryPoint =
            MakeNativeEntryPoint(Desc.EntryPoint);
        NSString* Name = [[NSString alloc]
            initWithBytes:NativeEntryPoint.View().data()
                   length:NativeEntryPoint.Len()
                 encoding:NSUTF8StringEncoding];
        id<MTLFunction> Function = [Library newFunctionWithName:Name];
        if (Function == nil || !MatchesFunctionType(Function.functionType, Desc.Stage))
            return {RHI::ERHIResult::InvalidState, nullptr};
        try
        {
            auto Object = Core::MakeShared<FMetalShaderLibrary>(
                Owner, Desc, Library, Function);
            return {RHI::ERHIResult::Success, std::move(Object)};
        }
        catch (const std::bad_alloc&)
        {
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}

} // namespace Stoner::Backend::Metal::Private
