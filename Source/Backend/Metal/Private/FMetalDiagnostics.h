#pragma once

#include "MetalRHI/FMetalBackendDiagnostics.h"

#include <mutex>

namespace Stoner::Backend::Metal::Private
{

class FMetalDiagnostics
{
public:
    static constexpr Core::usize MaxRecordCount = 128;
    static constexpr Core::usize MaxTextLength = 256;

    void Add(FMetalDiagnosticRecord Record) noexcept;
    [[nodiscard]] FMetalBackendDiagnostics Snapshot() const;
    [[nodiscard]] static Core::FString NormalizeText(
        const Core::FString& Text);

private:
    mutable std::mutex Mutex_;
    Core::TArray<FMetalDiagnosticRecord> Records_;
    bool bTruncated_ = false;
};

} // namespace Stoner::Backend::Metal::Private
