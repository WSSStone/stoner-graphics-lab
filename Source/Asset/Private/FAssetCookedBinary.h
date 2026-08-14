#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetSource.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <algorithm>
#include <bit>
#include <limits>
#include <optional>
#include <span>
#include <string_view>

namespace Stoner::Asset::Private
{

class FCookedBinaryWriter
{
public:
    explicit FCookedBinaryWriter(Core::uint64 MaximumBytes)
        : MaximumBytes_(MaximumBytes)
    {
    }

    void U8(Core::uint8 Value) { Append({&Value, 1}); }

    void U16(Core::uint16 Value)
    {
        Core::uint8 Bytes[2];
        for (Core::uint32 Index = 0; Index < 2; ++Index)
            Bytes[Index] = static_cast<Core::uint8>(Value >> (Index * 8U));
        Append(Bytes);
    }

    void U32(Core::uint32 Value)
    {
        Core::uint8 Bytes[4];
        for (Core::uint32 Index = 0; Index < 4; ++Index)
            Bytes[Index] = static_cast<Core::uint8>(Value >> (Index * 8U));
        Append(Bytes);
    }

    void U64(Core::uint64 Value)
    {
        Core::uint8 Bytes[8];
        for (Core::uint32 Index = 0; Index < 8; ++Index)
            Bytes[Index] = static_cast<Core::uint8>(Value >> (Index * 8U));
        Append(Bytes);
    }

    void Bool(bool Value) { U8(Value ? 1 : 0); }
    void Float(float Value) { U32(std::bit_cast<Core::uint32>(Value)); }

    void Text(const Core::FString& Value)
    {
        if (Value.IsEmpty() || Value.Len() > MaximumTextBytes ||
            Value.Len() > std::numeric_limits<Core::uint32>::max())
        {
            bValid_ = false;
            return;
        }
        U32(static_cast<Core::uint32>(Value.Len()));
        Append(std::span<const Core::uint8>(
            reinterpret_cast<const Core::uint8*>(Value.View().data()),
            Value.Len()));
    }

    void OptionalText(const std::optional<Core::FString>& Value)
    {
        Bool(Value.has_value());
        if (Value) Text(*Value);
    }

    void Bytes(std::span<const Core::uint8> Value)
    {
        if (Value.size() > std::numeric_limits<Core::uint64>::max())
        {
            bValid_ = false;
            return;
        }
        U64(static_cast<Core::uint64>(Value.size()));
        Append(Value);
    }

    void Digest(const FAssetDigest& Value)
    {
        if (!Value.IsAvailable())
        {
            bValid_ = false;
            return;
        }
        Append(Value.GetBytes());
    }

    void OptionalDigest(const std::optional<FAssetDigest>& Value)
    {
        Bool(Value.has_value());
        if (Value) Digest(*Value);
    }

    void AssetId(const FAssetId& Value)
    {
        if (!Value.IsValid())
        {
            bValid_ = false;
            return;
        }
        Text(Value.ToString());
    }

    void SourceLocator(const FAssetSourceLocator& Value)
    {
        if (!Value.IsValid())
        {
            bValid_ = false;
            return;
        }
        Text(Value.GetScheme());
        Text(Value.GetLocator());
    }

    [[nodiscard]] bool IsValid() const noexcept { return bValid_; }
    [[nodiscard]] Core::TArray<Core::uint8> Take()
    {
        return bValid_ ? std::move(Bytes_) : Core::TArray<Core::uint8>{};
    }

    static constexpr Core::uint32 MaximumTextBytes = 1024U * 1024U;

private:
    void Append(std::span<const Core::uint8> Value)
    {
        if (!bValid_ || Value.size() > MaximumBytes_ ||
            Bytes_.size() > MaximumBytes_ - Value.size())
        {
            bValid_ = false;
            return;
        }
        Bytes_.insert(Bytes_.end(), Value.begin(), Value.end());
    }

    Core::TArray<Core::uint8> Bytes_;
    Core::uint64 MaximumBytes_ = 0;
    bool bValid_ = true;
};

class FCookedBinaryReader
{
public:
    FCookedBinaryReader(
        std::span<const Core::uint8> Bytes,
        Core::uint32 MaximumElements = 100000,
        Core::uint32 MaximumTextBytes = FCookedBinaryWriter::MaximumTextBytes)
        : Bytes_(Bytes),
          MaximumElements_(MaximumElements),
          MaximumTextBytes_(MaximumTextBytes)
    {
    }

    bool U8(Core::uint8& Out)
    {
        if (!Require(1)) return false;
        Out = Bytes_[Offset_++];
        return true;
    }

    bool U16(Core::uint16& Out)
    {
        Core::uint64 Value = 0;
        if (!Unsigned(2, Value)) return false;
        Out = static_cast<Core::uint16>(Value);
        return true;
    }

    bool U32(Core::uint32& Out)
    {
        Core::uint64 Value = 0;
        if (!Unsigned(4, Value)) return false;
        Out = static_cast<Core::uint32>(Value);
        return true;
    }

    bool U64(Core::uint64& Out) { return Unsigned(8, Out); }

    bool Bool(bool& Out)
    {
        Core::uint8 Value = 0;
        if (!U8(Value) || Value > 1) return false;
        Out = Value != 0;
        return true;
    }

    bool Float(float& Out)
    {
        Core::uint32 Value = 0;
        if (!U32(Value)) return false;
        Out = std::bit_cast<float>(Value);
        return true;
    }

    bool Text(Core::FString& Out)
    {
        Core::uint32 Size = 0;
        if (!U32(Size) || Size == 0 || Size > MaximumTextBytes_ || !Require(Size))
            return false;
        const auto Begin = Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_);
        const auto End = Begin + static_cast<std::ptrdiff_t>(Size);
        if (std::find(Begin, End, Core::uint8{0}) != End) return false;
        Out = Core::FString(std::string_view(
            reinterpret_cast<const char*>(Bytes_.data() + Offset_), Size));
        Offset_ += Size;
        return true;
    }

    bool OptionalText(std::optional<Core::FString>& Out)
    {
        bool Present = false;
        if (!Bool(Present)) return false;
        Out.reset();
        if (!Present) return true;
        Core::FString Value;
        if (!Text(Value)) return false;
        Out = std::move(Value);
        return true;
    }

    bool Bytes(Core::TArray<Core::uint8>& Out)
    {
        Core::uint64 Size = 0;
        if (!U64(Size) || Size > Remaining() ||
            Size > std::numeric_limits<Core::usize>::max())
            return false;
        const auto Begin = Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset_);
        Out.assign(Begin, Begin + static_cast<std::ptrdiff_t>(Size));
        Offset_ += static_cast<Core::usize>(Size);
        return true;
    }

    bool Digest(FAssetDigest& Out)
    {
        if (!Require(32)) return false;
        constexpr char Hex[] = "0123456789abcdef";
        std::string TextValue(64, '0');
        for (Core::usize Index = 0; Index < 32; ++Index)
        {
            TextValue[Index * 2] = Hex[Bytes_[Offset_ + Index] >> 4U];
            TextValue[Index * 2 + 1] = Hex[Bytes_[Offset_ + Index] & 0x0fU];
        }
        Offset_ += 32;
        return FAssetDigest::ParseLowerHex(Core::FString(TextValue), Out) ==
            EAssetResult::Success;
    }

    bool OptionalDigest(std::optional<FAssetDigest>& Out)
    {
        bool Present = false;
        if (!Bool(Present)) return false;
        Out.reset();
        if (!Present) return true;
        FAssetDigest Value;
        if (!Digest(Value)) return false;
        Out = Value;
        return true;
    }

    bool AssetId(FAssetId& Out)
    {
        Core::FString TextValue;
        if (!Text(TextValue)) return false;
        const std::string_view View = TextValue.View();
        const std::size_t Colon = View.find(':');
        const std::size_t Hash = View.find(
            '#', Colon == std::string_view::npos ? 0 : Colon + 1);
        if (Colon == std::string_view::npos || Colon == 0 ||
            Colon + 1 >= View.size())
            return false;
        std::optional<Core::FString> Subresource;
        if (Hash != std::string_view::npos)
        {
            if (Hash + 1 >= View.size()) return false;
            Subresource = Core::FString(View.substr(Hash + 1));
        }
        if (FAssetId::Create(
                Core::FString(View.substr(0, Colon)),
                Core::FString(View.substr(
                    Colon + 1,
                    Hash == std::string_view::npos
                        ? std::string_view::npos : Hash - Colon - 1)),
                Subresource,
                Out) != EAssetResult::Success)
            return false;
        return Out.ToString() == TextValue;
    }

    bool SourceLocator(FAssetSourceLocator& Out)
    {
        Core::FString Scheme;
        Core::FString Locator;
        return Text(Scheme) && Text(Locator) &&
            FAssetSourceLocator::Create(Scheme, Locator, Out) ==
                EAssetResult::Success;
    }

    bool Count(Core::uint32& Out)
    {
        return U32(Out) && Out <= MaximumElements_;
    }

    [[nodiscard]] bool AtEnd() const noexcept { return Offset_ == Bytes_.size(); }
    [[nodiscard]] Core::usize Remaining() const noexcept
    {
        return Offset_ <= Bytes_.size() ? Bytes_.size() - Offset_ : 0;
    }

private:
    bool Unsigned(Core::usize Width, Core::uint64& Out)
    {
        if (!Require(Width)) return false;
        Out = 0;
        for (Core::usize Index = 0; Index < Width; ++Index)
            Out |= static_cast<Core::uint64>(Bytes_[Offset_ + Index]) <<
                (Index * 8U);
        Offset_ += Width;
        return true;
    }

    [[nodiscard]] bool Require(Core::usize Count) const noexcept
    {
        return Count <= Remaining();
    }

    std::span<const Core::uint8> Bytes_;
    Core::usize Offset_ = 0;
    Core::uint32 MaximumElements_ = 0;
    Core::uint32 MaximumTextBytes_ = 0;
};

} // namespace Stoner::Asset::Private
