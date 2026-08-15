#pragma once

#include "Asset/FAssetPayload.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetVersion.h"
#include "Core/TSharedPtr.h"

#include <functional>
#include <memory>
#include <utility>

namespace Stoner::Asset
{

namespace Private
{
class FAssetHandleControl
{
public:
    FAssetHandleControl(
        FAssetId Identity,
        FAssetVersion Version,
        Core::TSharedPtr<const FAssetPayload> Payload,
        std::function<void()> Release);
    ~FAssetHandleControl();

    [[nodiscard]] const FAssetId& GetIdentity() const noexcept;
    [[nodiscard]] const FAssetVersion& GetVersion() const noexcept;
    [[nodiscard]] const Core::TSharedPtr<const FAssetPayload>&
    GetPayload() const noexcept;

private:
    FAssetId Identity_;
    FAssetVersion Version_;
    Core::TSharedPtr<const FAssetPayload> Payload_;
    std::function<void()> Release_;
};
} // namespace Private

template <CTypedAssetPayload T>
class TAssetHandle
{
public:
    TAssetHandle() = default;

    [[nodiscard]] bool IsValid() const noexcept { return Get() != nullptr; }
    [[nodiscard]] const T* Get() const noexcept
    {
        return Control_
            ? dynamic_cast<const T*>(Control_->GetPayload().get())
            : nullptr;
    }
    [[nodiscard]] const T& operator*() const noexcept { return *Get(); }
    [[nodiscard]] const T* operator->() const noexcept { return Get(); }
    [[nodiscard]] const FAssetId& GetIdentity() const noexcept
    {
        return Control_->GetIdentity();
    }
    [[nodiscard]] const FAssetVersion& GetVersion() const noexcept
    {
        return Control_->GetVersion();
    }
    void Reset() noexcept { Control_.reset(); }

private:
    friend class FAssetManager;
    explicit TAssetHandle(Core::TSharedPtr<Private::FAssetHandleControl> Control)
        : Control_(std::move(Control))
    {
    }
    Core::TSharedPtr<Private::FAssetHandleControl> Control_;
};

} // namespace Stoner::Asset
