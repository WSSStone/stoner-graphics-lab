#include "Renderer/FShaderPermutation.h"

#include <algorithm>
#include <sstream>

namespace Stoner::Renderer
{

FShaderPermutation::FShaderPermutation(Stoner::Core::TArray<Stoner::Core::FString> InFlags)
{
    SetFlags(std::move(InFlags));
}

void FShaderPermutation::SetFlags(Stoner::Core::TArray<Stoner::Core::FString> InFlags)
{
    std::sort(InFlags.begin(), InFlags.end());
    InFlags.erase(std::unique(InFlags.begin(), InFlags.end()), InFlags.end());
    Flags = std::move(InFlags);
}

const Stoner::Core::TArray<Stoner::Core::FString>& FShaderPermutation::GetFlags() const noexcept
{
    return Flags;
}

Stoner::Core::FString FShaderPermutation::GetCanonicalKey() const
{
    if (Flags.empty())
    {
        return Stoner::Core::FString("<default>");
    }

    std::ostringstream Stream;
    for (std::size_t Index = 0; Index < Flags.size(); ++Index)
    {
        if (Index > 0)
        {
            Stream << '+';
        }
        Stream << Flags[Index].CStr();
    }
    return Stoner::Core::FString(Stream.str());
}

bool FShaderPermutation::HasFlag(const Stoner::Core::FString& Flag) const noexcept
{
    return std::find(Flags.begin(), Flags.end(), Flag) != Flags.end();
}

bool FShaderPermutation::IsEmpty() const noexcept
{
    return Flags.empty();
}

} // namespace Stoner::Renderer
