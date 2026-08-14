#include "FGLTFDependencyResolver.h"

#include <filesystem>
#include <array>
#include <string>

namespace Stoner::Asset::Private
{

EAssetResult DecodeGLTFDataUri(
    std::string_view Uri,
    Core::uint64 MaximumBytes,
    Core::TArray<Core::uint8>& OutBytes)
{
    OutBytes.clear();
    const std::size_t Comma = Uri.find(',');
    if (!Uri.starts_with("data:") || Comma == std::string_view::npos ||
        !Uri.substr(0, Comma).ends_with(";base64"))
        return EAssetResult::AccessDenied;
    const std::string_view Encoded = Uri.substr(Comma + 1);
    if (Encoded.empty() || Encoded.size() % 4 != 0 ||
        Encoded.size() / 4 > MaximumBytes / 3 + 1)
        return EAssetResult::CapacityExceeded;
    const auto Decode = [](char Character)
    {
        if (Character >= 'A' && Character <= 'Z') return Character - 'A';
        if (Character >= 'a' && Character <= 'z') return Character - 'a' + 26;
        if (Character >= '0' && Character <= '9') return Character - '0' + 52;
        if (Character == '+') return 62;
        if (Character == '/') return 63;
        return -1;
    };
    OutBytes.reserve(Encoded.size() / 4 * 3);
    for (std::size_t Offset = 0; Offset < Encoded.size(); Offset += 4)
    {
        std::array<int, 4> Values{};
        int Padding = 0;
        for (std::size_t Index = 0; Index < 4; ++Index)
        {
            const char Character = Encoded[Offset + Index];
            if (Character == '=') { Values[Index] = 0; ++Padding; }
            else if (Padding != 0 || (Values[Index] = Decode(Character)) < 0)
            { OutBytes.clear(); return EAssetResult::MalformedSource; }
        }
        if (Padding > 2 || (Padding != 0 && Offset + 4 != Encoded.size()))
        { OutBytes.clear(); return EAssetResult::MalformedSource; }
        const Core::uint32 Value =
            (static_cast<Core::uint32>(Values[0]) << 18U) |
            (static_cast<Core::uint32>(Values[1]) << 12U) |
            (static_cast<Core::uint32>(Values[2]) << 6U) |
            static_cast<Core::uint32>(Values[3]);
        OutBytes.push_back(static_cast<Core::uint8>(Value >> 16U));
        if (Padding < 2) OutBytes.push_back(static_cast<Core::uint8>(Value >> 8U));
        if (Padding < 1) OutBytes.push_back(static_cast<Core::uint8>(Value));
    }
    return OutBytes.size() <= MaximumBytes
        ? EAssetResult::Success : EAssetResult::CapacityExceeded;
}

EAssetResult ResolveGLTFDependency(
    const FAssetSourceLocator& MainSource,
    std::string_view RelativeUri,
    const Core::TSharedPtr<IAssetResolver>& Resolver,
    Core::uint64 MaximumBytes,
    FGLTFResolvedDependency& OutDependency)
{
    OutDependency = {};
    if (!MainSource.IsValid() || !Resolver || RelativeUri.empty() ||
        MaximumBytes == 0 || RelativeUri.find('%') != std::string_view::npos ||
        RelativeUri.find('\\') != std::string_view::npos ||
        RelativeUri.find('?') != std::string_view::npos ||
        RelativeUri.find('#') != std::string_view::npos ||
        RelativeUri.find(':') != std::string_view::npos)
        return EAssetResult::AccessDenied;

    const std::filesystem::path SourcePath(MainSource.GetLocator().ToStdString());
    const std::filesystem::path Scope = SourcePath.parent_path().lexically_normal();
    const std::filesystem::path Relative(RelativeUri);
    if (Relative.is_absolute()) return EAssetResult::AccessDenied;
    const std::filesystem::path Combined = (Scope / Relative).lexically_normal();
    const std::filesystem::path RelativeToScope = Combined.lexically_relative(Scope);
    if (RelativeToScope.empty() || *RelativeToScope.begin() == ".." ||
        Combined == SourcePath.lexically_normal())
        return EAssetResult::AccessDenied;

    FAssetSourceLocator Location;
    EAssetResult Result = FAssetSourceLocator::Create(
        MainSource.GetScheme(), Core::FString(Combined.generic_string()), Location);
    if (Result != EAssetResult::Success) return Result;
    FAssetResolveResult Resolved = Resolver->Resolve({Location});
    if (Resolved.Result != EAssetResult::Success) return Resolved.Result;
    if (!Resolved.Descriptor.Location.IsValid() || !Resolved.Source.IsValid() ||
        Resolved.Descriptor.Location.GetScheme() != MainSource.GetScheme())
        return EAssetResult::AccessDenied;
    const std::filesystem::path ResolvedPath(
        Resolved.Descriptor.Location.GetLocator().ToStdString());
    const std::filesystem::path ResolvedRelative =
        ResolvedPath.lexically_normal().lexically_relative(Scope);
    if (ResolvedRelative.empty() || *ResolvedRelative.begin() == "..")
        return EAssetResult::AccessDenied;
    Result = Resolved.Source.ReadBounded(
        MaximumBytes, Resolved.Descriptor.Size, OutDependency.Bytes);
    if (Result != EAssetResult::Success) return Result;
    OutDependency.Descriptor = std::move(Resolved.Descriptor);
    OutDependency.Source = std::move(Resolved.Source);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
