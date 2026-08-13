#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FStaticModelImport.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <span>

struct cgltf_data;

namespace Stoner::Asset::Private
{

struct FCgltfAllocationState;

class FCgltfDocument
{
public:
    FCgltfDocument() = default;
    ~FCgltfDocument();

    FCgltfDocument(const FCgltfDocument&) = delete;
    FCgltfDocument& operator=(const FCgltfDocument&) = delete;
    FCgltfDocument(FCgltfDocument&& Other) noexcept;
    FCgltfDocument& operator=(FCgltfDocument&& Other) noexcept;

    [[nodiscard]] static EAssetResult Parse(
        std::span<const Core::uint8> SourceBytes,
        const FStaticModelImportProfile& Profile,
        FCgltfDocument& OutDocument,
        FAssetDiagnosticList* OutDiagnostics = nullptr);

    [[nodiscard]] const cgltf_data* GetNativeDocument() const noexcept;
    [[nodiscard]] Core::uint64 GetParserAllocatedBytes() const noexcept;

private:
    void Reset() noexcept;

    Core::TArray<Core::uint8> SourceBytes_;
    Core::TSharedPtr<FCgltfAllocationState> AllocationState_;
    cgltf_data* Data_ = nullptr;
};

} // namespace Stoner::Asset::Private
