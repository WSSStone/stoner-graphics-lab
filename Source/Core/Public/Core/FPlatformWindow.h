#pragma once

namespace Stoner::Core
{

class FPlatformWindow
{
public:
    FPlatformWindow() = default;

    explicit FPlatformWindow(void* NativeHandle) noexcept
        : NativeHandle_(NativeHandle)
    {
    }

    [[nodiscard]] bool IsValid() const noexcept
    {
        return NativeHandle_ != nullptr;
    }

    [[nodiscard]] void* GetNativeHandle() const noexcept
    {
        return NativeHandle_;
    }

    void Clear() noexcept
    {
        NativeHandle_ = nullptr;
    }

private:
    void* NativeHandle_ = nullptr;
};

} // namespace Stoner::Core
