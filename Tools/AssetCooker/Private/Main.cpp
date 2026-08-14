#include "AssetCooker/AssetCookerMinimal.h"
#include "FAssetCookCli.h"
#include "FAssetCookReportCodec.h"

#include <iostream>

int main(int ArgCount, char* Arguments[])
{
    using namespace Stoner;
    Core::TArray<Core::FString> Values;
    for (int Index = 1; Index < ArgCount; ++Index)
        Values.emplace_back(Arguments[Index]);
    AssetCooker::Private::FAssetCookCliInvocation Invocation;
    Core::FString Reason;
    const auto Parsed = AssetCooker::Private::FAssetCookCli::Parse(
        Values, Invocation, Reason);
    if (Parsed != AssetCooker::EAssetCookResultCategory::Success)
    {
        std::cerr << AssetCooker::GetAssetCookerName().ToStdString()
                  << ": invalid-arguments: " << Reason.ToStdString() << '\n';
        return AssetCooker::Private::FAssetCookReportCodec::ExitCode(Parsed);
    }
    const auto Result =
        AssetCooker::Private::FAssetCookCli::Execute(Invocation);
    std::ostream& Stream = Result.Succeeded() ? std::cout : std::cerr;
    Stream << AssetCooker::GetAssetCookerName().ToStdString() << ": "
           << AssetCooker::Private::FAssetCookReportCodec::ResultToken(
                  Result.Category)
           << ": " << Result.StableReason.ToStdString() << '\n';
    if (Invocation.ReportPath.IsEmpty() && !Result.CanonicalReport.IsEmpty())
        std::cout << Result.CanonicalReport.ToStdString();
    return AssetCooker::Private::FAssetCookReportCodec::ExitCode(
        Result.Category);
}
