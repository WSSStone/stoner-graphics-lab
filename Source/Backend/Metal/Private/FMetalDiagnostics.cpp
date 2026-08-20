#include "FMetalDiagnostics.h"

#include <cctype>
#include <string>

namespace Stoner::Backend::Metal::Private
{
namespace
{

bool IsHex(char Value) noexcept
{
    return std::isxdigit(static_cast<unsigned char>(Value)) != 0;
}

std::string RedactNativeAddresses(std::string Text)
{
    std::string Result;
    Result.reserve(Text.size());
    for (Core::usize Index = 0; Index < Text.size();)
    {
        if (Index + 2 <= Text.size() && Text[Index] == '0' &&
            Text[Index + 1] == 'x')
        {
            Core::usize End = Index + 2;
            while (End < Text.size() && IsHex(Text[End])) ++End;
            if (End - Index >= 6)
            {
                Result += "<native-address>";
                Index = End;
                continue;
            }
        }
        Result.push_back(Text[Index++]);
    }
    return Result;
}

} // namespace

Core::FString FMetalDiagnostics::NormalizeText(const Core::FString& Text)
{
    std::string Normalized = RedactNativeAddresses(Text.ToStdString());
    for (char& Value : Normalized)
        if (Value == '\r' || Value == '\n' || Value == '\t') Value = ' ';
    if (Normalized.size() > MaxTextLength)
        Normalized.resize(MaxTextLength);
    return Core::FString(std::move(Normalized));
}

void FMetalDiagnostics::Add(FMetalDiagnosticRecord Record) noexcept
{
    try
    {
        Record.Backend = Core::FString("Metal");
        Record.Operation = NormalizeText(Record.Operation);
        Record.Context = NormalizeText(Record.Context);
        Record.StableReason = NormalizeText(Record.StableReason);
        Record.CapabilityReason = NormalizeText(Record.CapabilityReason);
        Record.RecoveryState = NormalizeText(Record.RecoveryState);
        std::lock_guard Lock(Mutex_);
        if (Records_.size() >= MaxRecordCount)
        {
            bTruncated_ = true;
            return;
        }
        Records_.push_back(std::move(Record));
    }
    catch (...)
    {
        std::lock_guard Lock(Mutex_);
        bTruncated_ = true;
    }
}

FMetalBackendDiagnostics FMetalDiagnostics::Snapshot() const
{
    std::lock_guard Lock(Mutex_);
    FMetalBackendDiagnostics Result;
    Result.Records = Records_;
    Result.bTruncated = bTruncated_;
    return Result;
}

} // namespace Stoner::Backend::Metal::Private
