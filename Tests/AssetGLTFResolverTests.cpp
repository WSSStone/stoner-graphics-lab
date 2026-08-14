#include "AssetGLTFResolverTests.h"

#include "StaticModelTestSupport.h"

#include <iostream>
#include <string>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetGLTFResolverTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

class FControlledResolver final : public IAssetResolver
{
public:
    explicit FControlledResolver(bool Alias = false) : Alias_(Alias) {}

    FAssetExtensionCapability GetCapability() const override { return {}; }

    FAssetResolveResult Resolve(const FAssetResolveRequest& Request) override
    {
        ++Calls;
        FAssetResolveResult Result;
        const auto Bytes = ReadFixture(
            "Tests/Fixtures/StaticModel/Valid/Materials/albedo.jpg");
        Result.Result = EAssetResult::Success;
        Result.Descriptor.Location = Request.Location;
        if (Alias_)
            (void)FAssetSourceLocator::Create(FString("fixture"),
                FString("Tests/Fixtures/StaticModel/Valid/Materials/alias.jpg"),
                Result.Descriptor.Location);
        Result.Descriptor.Size = Bytes.size();
        Result.Descriptor.FormatHint = FString("jpeg");
        Result.Source = FAssetSourceLease(
            MakeShared<FMemorySource>(Bytes));
        return Result;
    }

    int Calls = 0;

private:
    bool Alias_ = false;
};

EAssetResult ImportWithUri(const std::string& Uri,
    const TSharedPtr<IAssetResolver>& Resolver, TArray<FAssetImportOutput>& Outputs)
{
    auto Bytes = ReadFixture(
        "Tests/Fixtures/StaticModel/Valid/Materials/02-external-jpeg.gltf");
    std::string Text(Bytes.begin(), Bytes.end());
    const std::size_t At = Text.find("albedo.jpg");
    Text.replace(At, std::string("albedo.jpg").size(), Uri);
    FStaticModelImportRequest Request;
    Request.AssetRequest = MakeMemoryRequest(
        TArray<uint8>(Text.begin(), Text.end()),
        FString("Tests/Fixtures/StaticModel/Valid/Materials/model.gltf"));
    Request.Profile = MakeShared<FStaticModelImportProfile>();
    Request.DependencyResolver = Resolver;
    return ImportStaticModel(Request, Outputs);
}
}

FAssetGLTFResolverTestResult RunAssetGLTFResolverTests()
{
    FAssetGLTFResolverTestResult Result;
    for (const std::string& Uri : {
             std::string("../albedo.jpg"), std::string("%2e%2e/albedo.jpg"),
             std::string("https://example.test/albedo.jpg"),
             std::string("model.gltf"), std::string("albedo.jpg#fragment")})
    {
        auto Resolver = MakeShared<FControlledResolver>();
        TArray<FAssetImportOutput> Outputs;
        const EAssetResult ImportResult = ImportWithUri(Uri, Resolver, Outputs);
        Record(Result,
            ImportResult == EAssetResult::AccessDenied && Outputs.empty() &&
                Resolver->Calls == 0,
            "unsafe dependency locator rejects before resolver callback");
    }

    auto AliasResolver = MakeShared<FControlledResolver>(true);
    TArray<FAssetImportOutput> AliasOutputs;
    Record(Result,
        ImportWithUri("albedo.jpg", AliasResolver, AliasOutputs) ==
            EAssetResult::AccessDenied && AliasOutputs.empty() &&
            AliasResolver->Calls == 1,
        "resolver alias cannot redirect a canonical dependency identity");

    auto ExactResolver = MakeShared<FControlledResolver>();
    TArray<FAssetImportOutput> ExactOutputs;
    Record(Result,
        ImportWithUri("albedo.jpg", ExactResolver, ExactOutputs) ==
            EAssetResult::Success && !ExactOutputs.empty() &&
            ExactResolver->Calls == 1,
        "exact source-relative dependency resolves within scope");
    return Result;
}
