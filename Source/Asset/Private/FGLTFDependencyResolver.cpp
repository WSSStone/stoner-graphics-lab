#include "FGLTFDependencyResolver.h"

#include <array>
#include <filesystem>
#include <string>

namespace Stoner::Asset::Private
{

namespace
{
bool IsWithinSourceScope(
    const std::filesystem::path& Candidate,
    const std::filesystem::path& Scope)
{
    const std::filesystem::path Normalized = Candidate.lexically_normal();
    if (Normalized.empty()) return false;
    if (Scope.empty()) return *Normalized.begin() != "..";
    const std::filesystem::path Relative =
        Normalized.lexically_relative(Scope);
    return !Relative.empty() && *Relative.begin() != "..";
}
} // namespace

EAssetResult DecodeGLTFDataUri(
    std::string_view Uri,
    Core::uint64 MaximumBytes,
    Core::TArray<Core::uint8>& OutBytes)
{
    OutBytes.clear();
    const std::size_t Comma = Uri.find(',');
    const std::string_view Header = Uri.substr(0, Comma);
    if (!Uri.starts_with("data:") || Comma == std::string_view::npos ||
        Header.size() <= 5 || !Header.ends_with(";base64") ||
        Header.find_first_of("\r\n\t ") != std::string_view::npos)
        return EAssetResult::AccessDenied;
    const std::string_view Encoded = Uri.substr(Comma + 1);
    if (Encoded.empty() || Encoded.size() % 4 != 0)
        return EAssetResult::MalformedSource;
    if (Encoded.size() / 4 > MaximumBytes / 3 + 1)
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
        if (Padding > 2 || Values[0] < 0 || Values[1] < 0 ||
            Encoded[Offset] == '=' || Encoded[Offset + 1] == '=' ||
            (Padding == 2 && (Encoded[Offset + 2] != '=' ||
                Encoded[Offset + 3] != '=' || (Values[1] & 0x0f) != 0)) ||
            (Padding == 1 && (Encoded[Offset + 3] != '=' ||
                Encoded[Offset + 2] == '=' || (Values[2] & 0x03) != 0)) ||
            (Padding != 0 && Offset + 4 != Encoded.size()))
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
    if (!IsWithinSourceScope(Combined, Scope) ||
        Combined == SourcePath.lexically_normal())
        return EAssetResult::AccessDenied;

    FAssetSourceLocator Location;
    EAssetResult Result = FAssetSourceLocator::Create(
        MainSource.GetScheme(), Core::FString(Combined.generic_string()), Location);
    if (Result != EAssetResult::Success) return Result;
    FAssetResolveResult Resolved = Resolver->Resolve({Location, {}});
    if (Resolved.Result != EAssetResult::Success) return Resolved.Result;
    if (!Resolved.Descriptor.Location.IsValid() || !Resolved.Source.IsValid() ||
        Resolved.Descriptor.Location != Location)
        return EAssetResult::AccessDenied;
    const std::filesystem::path ResolvedPath(
        Resolved.Descriptor.Location.GetLocator().ToStdString());
    if (!IsWithinSourceScope(ResolvedPath, Scope) ||
        ResolvedPath.lexically_normal() == SourcePath.lexically_normal())
        return EAssetResult::AccessDenied;
    Result = Resolved.Source.ReadBounded(
        MaximumBytes, Resolved.Descriptor.Size, OutDependency.Bytes);
    if (Result != EAssetResult::Success) return Result;
    OutDependency.Descriptor = std::move(Resolved.Descriptor);
    OutDependency.Source = std::move(Resolved.Source);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
