#include "FCgltfDocument.h"

#include "FGLTFContainerPreflight.h"
#include "FGLTFDiagnostics.h"

#include "../../../ThirdParty/cgltf/cgltf.h"

#include <cstdlib>
#include <limits>
#include <utility>

namespace Stoner::Asset::Private
{

struct FCgltfAllocationState
{
    Core::uint64 Limit = 0;
    Core::uint64 Allocated = 0;
};

namespace
{

struct FAllocationHeader
{
    Core::uint64 Size = 0;
};

void* CgltfAllocate(void* UserData, cgltf_size Size)
{
    auto* State = static_cast<FCgltfAllocationState*>(UserData);
    if (State == nullptr || Size > std::numeric_limits<Core::uint64>::max() ||
        Size > State->Limit - State->Allocated ||
        Size > std::numeric_limits<std::size_t>::max() - sizeof(FAllocationHeader))
    {
        return nullptr;
    }
    void* Raw = std::malloc(static_cast<std::size_t>(Size) + sizeof(FAllocationHeader));
    if (Raw == nullptr)
    {
        return nullptr;
    }
    auto* Header = static_cast<FAllocationHeader*>(Raw);
    Header->Size = static_cast<Core::uint64>(Size);
    State->Allocated += Header->Size;
    return Header + 1;
}

void CgltfFree(void* UserData, void* Pointer)
{
    if (Pointer == nullptr)
    {
        return;
    }
    auto* State = static_cast<FCgltfAllocationState*>(UserData);
    auto* Header = static_cast<FAllocationHeader*>(Pointer) - 1;
    if (State != nullptr && Header->Size <= State->Allocated)
    {
        State->Allocated -= Header->Size;
    }
    std::free(Header);
}

cgltf_result RejectFileRead(
    const cgltf_memory_options*,
    const cgltf_file_options*,
    const char*,
    cgltf_size*,
    void**)
{
    return cgltf_result_io_error;
}

void IgnoreFileRelease(
    const cgltf_memory_options*,
    const cgltf_file_options*,
    void*,
    cgltf_size)
{
}

EAssetResult ToAssetResult(cgltf_result Result) noexcept
{
    if (Result == cgltf_result_out_of_memory)
    {
        return EAssetResult::CapacityExceeded;
    }
    return EAssetResult::MalformedSource;
}

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    Core::uint32 MaximumDiagnostics,
    EAssetResult Result,
    const char* Field)
{
    AppendGLTFDiagnostic(Diagnostics, MaximumDiagnostics,
        EAssetStage::Parse, Result, EAssetDiagnosticSeverity::Error,
        Core::FString("asset.gltf.parse"), Core::FString("parser.cgltf"),
        {}, Core::FString(Field), Core::FString("bounded parse failed"));
}

} // namespace

FCgltfDocument::~FCgltfDocument()
{
    Reset();
}

FCgltfDocument::FCgltfDocument(FCgltfDocument&& Other) noexcept
    : SourceBytes_(std::move(Other.SourceBytes_))
    , AllocationState_(std::move(Other.AllocationState_))
    , Data_(std::exchange(Other.Data_, nullptr))
{
}

FCgltfDocument& FCgltfDocument::operator=(FCgltfDocument&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        SourceBytes_ = std::move(Other.SourceBytes_);
        AllocationState_ = std::move(Other.AllocationState_);
        Data_ = std::exchange(Other.Data_, nullptr);
    }
    return *this;
}

EAssetResult FCgltfDocument::Parse(
    std::span<const Core::uint8> SourceBytes,
    const FStaticModelImportProfile& Profile,
    FCgltfDocument& OutDocument,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutDocument.Reset();
    if (OutDiagnostics != nullptr)
    {
        OutDiagnostics->clear();
    }
    if (Profile.Validate() != EAssetResult::Success)
    {
        AddDiagnostic(OutDiagnostics, 1, EAssetResult::InvalidInput, "profile");
        return EAssetResult::InvalidInput;
    }
    FGLTFContainerPreflightResult Preflight;
    const EAssetResult PreflightResult = PreflightGLTFContainer(
        SourceBytes, Profile.Limits, Preflight, OutDiagnostics);
    if (PreflightResult != EAssetResult::Success)
    {
        return PreflightResult;
    }

    OutDocument.SourceBytes_.assign(SourceBytes.begin(), SourceBytes.end());
    OutDocument.AllocationState_ = Core::MakeShared<FCgltfAllocationState>();
    OutDocument.AllocationState_->Limit = Profile.Limits.MaxParserAllocationBytes;

    cgltf_options Options{};
    Options.type = Preflight.Type == EGLTFContainerType::GLB
        ? cgltf_file_type_glb
        : cgltf_file_type_gltf;
    Options.memory.alloc_func = CgltfAllocate;
    Options.memory.free_func = CgltfFree;
    Options.memory.user_data = OutDocument.AllocationState_.get();
    Options.file.read = RejectFileRead;
    Options.file.release = IgnoreFileRelease;

    cgltf_data* Parsed = nullptr;
    const cgltf_result ParseResult = cgltf_parse(
        &Options,
        OutDocument.SourceBytes_.data(),
        OutDocument.SourceBytes_.size(),
        &Parsed);
    if (ParseResult != cgltf_result_success || Parsed == nullptr)
    {
        const EAssetResult Result = ToAssetResult(ParseResult);
        AddDiagnostic(OutDiagnostics, Profile.Limits.MaxDiagnostics,
            Result, "source");
        OutDocument.Reset();
        return Result;
    }
    OutDocument.Data_ = Parsed;
    return EAssetResult::Success;
}

const cgltf_data* FCgltfDocument::GetNativeDocument() const noexcept
{
    return Data_;
}

Core::uint64 FCgltfDocument::GetParserAllocatedBytes() const noexcept
{
    return AllocationState_ ? AllocationState_->Allocated : 0;
}

void FCgltfDocument::Reset() noexcept
{
    if (Data_ != nullptr)
    {
        cgltf_free(Data_);
        Data_ = nullptr;
    }
    AllocationState_.reset();
    SourceBytes_.clear();
}

} // namespace Stoner::Asset::Private
