#include "Asset/FAssetId.h"

#include "Core/FUnicode.h"

#include <cctype>
#include <functional>
#include <string>
#include <vector>

namespace Stoner::Asset
{
namespace
{

bool IsControl(unsigned char Character)
{
    return Character < 0x20U || Character == 0x7fU;
}

bool IsValidAssetType(std::string_view Text)
{
    if (Text.empty() || Text.size() > 255)
    {
        return false;
    }
    const auto IsAlpha = [](unsigned char Character)
    {
        return (Character >= 'A' && Character <= 'Z') ||
            (Character >= 'a' && Character <= 'z');
    };
    if (!IsAlpha(static_cast<unsigned char>(Text.front())))
    {
        return false;
    }
    for (const unsigned char Character : Text)
    {
        if (!IsAlpha(Character) && !(Character >= '0' && Character <= '9') &&
            Character != '_' && Character != '.' && Character != '-')
        {
            return false;
        }
    }
    return true;
}

EAssetResult NormalizeComponent(
    const Core::FString& Input,
    Core::FString& Output,
    bool AllowSeparators)
{
    const Core::EUnicodeResult UnicodeResult =
        Core::FUnicode::NormalizeNFC(Input, Output);
    if (UnicodeResult == Core::EUnicodeResult::InvalidUtf8)
    {
        return EAssetResult::InvalidUtf8;
    }
    if (UnicodeResult != Core::EUnicodeResult::Success)
    {
        return EAssetResult::ProcessingFailure;
    }

    for (const unsigned char Character : Output.View())
    {
        if (Character == 0 || IsControl(Character) || Character == ':' ||
            Character == '#' || (!AllowSeparators && (Character == '/' || Character == '\\')))
        {
            Output.Clear();
            return EAssetResult::InvalidIdentity;
        }
    }
    return EAssetResult::Success;
}

EAssetResult NormalizePath(const Core::FString& Input, Core::FString& Output)
{
    Core::FString Normalized;
    EAssetResult Result = NormalizeComponent(Input, Normalized, true);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }

    std::string Text = Normalized.ToStdString();
    if (Text.empty() || Text.front() == '/' || Text.front() == '\\' ||
        Text.back() == '/' || Text.back() == '\\')
    {
        return EAssetResult::InvalidIdentity;
    }
    for (char& Character : Text)
    {
        if (Character == '\\')
        {
            Character = '/';
        }
    }

    std::vector<std::string> Segments;
    std::size_t Begin = 0;
    while (Begin <= Text.size())
    {
        const std::size_t End = Text.find('/', Begin);
        const std::string Segment = Text.substr(
            Begin,
            End == std::string::npos ? std::string::npos : End - Begin);
        if (Segment == "..")
        {
            return EAssetResult::InvalidIdentity;
        }
        if (!Segment.empty() && Segment != ".")
        {
            if (Segment.size() > 255)
            {
                return EAssetResult::IdentityTooLong;
            }
            Segments.push_back(Segment);
        }
        if (End == std::string::npos)
        {
            break;
        }
        Begin = End + 1;
    }
    if (Segments.empty())
    {
        return EAssetResult::InvalidIdentity;
    }

    std::string Canonical;
    for (const std::string& Segment : Segments)
    {
        if (!Canonical.empty())
        {
            Canonical.push_back('/');
        }
        Canonical += Segment;
    }
    Output = Core::FString(std::move(Canonical));
    return EAssetResult::Success;
}

} // namespace

EAssetResult FAssetId::Create(
    const Core::FString& AssetType,
    const Core::FString& LogicalPath,
    const std::optional<Core::FString>& Subresource,
    FAssetId& OutId)
{
    OutId = {};
    if (!IsValidAssetType(AssetType.View()))
    {
        return AssetType.Len() > 255
            ? EAssetResult::IdentityTooLong
            : EAssetResult::InvalidIdentity;
    }

    Core::FString CanonicalPath;
    EAssetResult Result = NormalizePath(LogicalPath, CanonicalPath);
    if (Result != EAssetResult::Success)
    {
        return Result;
    }

    std::optional<Core::FString> CanonicalSubresource;
    if (Subresource.has_value())
    {
        Core::FString Value;
        Result = NormalizeComponent(*Subresource, Value, false);
        if (Result != EAssetResult::Success)
        {
            return Result;
        }
        if (Value.IsEmpty())
        {
            return EAssetResult::InvalidIdentity;
        }
        if (Value.Len() > 255)
        {
            return EAssetResult::IdentityTooLong;
        }
        CanonicalSubresource = std::move(Value);
    }

    const std::size_t FullLength = AssetType.Len() + 1 + CanonicalPath.Len() +
        (CanonicalSubresource ? 1 + CanonicalSubresource->Len() : 0);
    if (FullLength > 1024)
    {
        return EAssetResult::IdentityTooLong;
    }

    OutId.AssetType_ = AssetType;
    OutId.LogicalPath_ = std::move(CanonicalPath);
    OutId.Subresource_ = std::move(CanonicalSubresource);
    return EAssetResult::Success;
}

bool FAssetId::IsValid() const noexcept
{
    return !AssetType_.IsEmpty() && !LogicalPath_.IsEmpty();
}

const Core::FString& FAssetId::GetAssetType() const noexcept
{
    return AssetType_;
}

const Core::FString& FAssetId::GetLogicalPath() const noexcept
{
    return LogicalPath_;
}

const std::optional<Core::FString>& FAssetId::GetSubresource() const noexcept
{
    return Subresource_;
}

Core::FString FAssetId::ToString() const
{
    std::string Text = AssetType_.ToStdString() + ":" + LogicalPath_.ToStdString();
    if (Subresource_)
    {
        Text += "#" + Subresource_->ToStdString();
    }
    return Core::FString(std::move(Text));
}

std::size_t FAssetId::GetLookupHash() const noexcept
{
    std::size_t Hash = std::hash<std::string_view>{}(AssetType_.View());
    Hash ^= std::hash<std::string_view>{}(LogicalPath_.View()) +
        0x9e3779b9U + (Hash << 6U) + (Hash >> 2U);
    if (Subresource_)
    {
        Hash ^= std::hash<std::string_view>{}(Subresource_->View()) +
            0x9e3779b9U + (Hash << 6U) + (Hash >> 2U);
    }
    return Hash;
}

bool FAssetId::CompareWithForcedCommonHashForTesting(
    const FAssetId& Left,
    const FAssetId& Right,
    std::size_t CommonHash) noexcept
{
    (void)CommonHash;
    return Left == Right;
}

bool FAssetId::operator<(const FAssetId& Other) const noexcept
{
    if (AssetType_ != Other.AssetType_)
    {
        return AssetType_ < Other.AssetType_;
    }
    if (LogicalPath_ != Other.LogicalPath_)
    {
        return LogicalPath_ < Other.LogicalPath_;
    }
    return Subresource_ < Other.Subresource_;
}

} // namespace Stoner::Asset
