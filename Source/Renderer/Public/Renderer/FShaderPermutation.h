#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Renderer
{

class FShaderPermutation
{
public:
    FShaderPermutation() = default;
    explicit FShaderPermutation(Stoner::Core::TArray<Stoner::Core::FString> InFlags);

    void SetFlags(Stoner::Core::TArray<Stoner::Core::FString> InFlags);
    [[nodiscard]] const Stoner::Core::TArray<Stoner::Core::FString>& GetFlags() const noexcept;
    [[nodiscard]] Stoner::Core::FString GetCanonicalKey() const;
    [[nodiscard]] bool HasFlag(const Stoner::Core::FString& Flag) const noexcept;
    [[nodiscard]] bool IsEmpty() const noexcept;

    [[nodiscard]] friend bool operator==(const FShaderPermutation& Left, const FShaderPermutation& Right)
    {
        return Left.Flags == Right.Flags;
    }

private:
    Stoner::Core::TArray<Stoner::Core::FString> Flags;
};

} // namespace Stoner::Renderer
