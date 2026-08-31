#include "ProductionContentTests.h"

#include "ProductionContentTestSupport.h"
#include "../ThirdParty/yyjson/yyjson.h"

#include <array>
#include <filesystem>
#include <iostream>
#include <set>

namespace
{

void Record(
    FProductionContentTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestProfileScaffolding(FProductionContentTestResult& Result)
{
    constexpr std::array<const char*, 3> Paths = {
        "Config/Validation/ProductionContent/Regular.json",
        "Config/Validation/ProductionContent/Medium.json",
        "Config/Validation/ProductionContent/Hardware.json"};
    bool bValid = true;
    for (const char* Path : Paths)
    {
        const std::string Text =
            ProductionContentTestSupport::ReadText(Path);
        bValid = bValid && ProductionContentTestSupport::ContainsAll(
            Text,
            {"stoner.production-validation-profile", "\"schemaVersion\": 4",
             "packageLifecycles", "packageId", "purpose", "cycles",
             "warmupCycles", "maxRssGrowthBytes", "requiredGates"});
    }
    Record(Result, bValid, "production validation profile scaffolding exists");
}

void TestOutOfBandMaintainerNote(FProductionContentTestResult& Result)
{
    const std::string Readme = ProductionContentTestSupport::ReadText(
        "Content/ProductionAcceptance/README.md");
    Record(
        Result,
        std::filesystem::is_regular_file(
            "Content/ProductionAcceptance/MAINTAINER_NOTES.md") &&
            ProductionContentTestSupport::ContainsAll(
                Readme,
                {"never", "open", "hash", "acceptance decision"}),
        "maintainer note is explicitly outside automated acceptance");
}

void TestDeviceClassRegistryScaffolding(
    FProductionContentTestResult& Result)
{
    const std::string Text = ProductionContentTestSupport::ReadText(
        "Config/Validation/ProductionContent/DeviceClasses.json");
    Record(
        Result,
        ProductionContentTestSupport::ContainsAll(
            Text,
            {"stoner.production-device-class-registry",
             "backendImplementation", "cpuArchitecture", "adapterFamily",
             "shaderProfile", "colorFormat", "depthFormat", "sampleCount",
             "textureFormatFamily"}),
        "device class registry contains the canonical signature fields");
}

void TestFailureCatalog(FProductionContentTestResult& Result)
{
    std::string Text = ProductionContentTestSupport::ReadText(
        "Tests/Fixtures/ProductionContent/Failures/failure-catalog.json");
    yyjson_doc* Document = yyjson_read_opts(
        Text.data(), Text.size(), YYJSON_READ_NOFLAG, nullptr, nullptr);
    yyjson_val* Root = Document ? yyjson_doc_get_root(Document) : nullptr;
    yyjson_val* Cases = Root ? yyjson_obj_get(Root, "cases") : nullptr;
    std::set<std::string> Ids;
    std::set<std::string> Stages;
    bool bValid = yyjson_is_obj(Root) && yyjson_obj_size(Root) == 3 &&
        yyjson_is_arr(Cases) && yyjson_arr_size(Cases) >= 30;
    size_t Index = 0;
    size_t Maximum = 0;
    yyjson_val* Case = nullptr;
    if (bValid)
    {
        yyjson_arr_foreach(Cases, Index, Maximum, Case)
        {
            yyjson_val* Id = yyjson_obj_get(Case, "caseId");
            yyjson_val* Stage = yyjson_obj_get(Case, "stage");
            yyjson_val* Category = yyjson_obj_get(Case, "expectedCategory");
            yyjson_val* Profile = yyjson_obj_get(Case, "reproductionProfile");
            bValid = yyjson_is_obj(Case) && yyjson_obj_size(Case) == 4 &&
                yyjson_is_str(Id) && yyjson_is_str(Stage) &&
                yyjson_is_str(Category) && yyjson_is_str(Profile) &&
                yyjson_get_len(Category) > 0 && yyjson_get_len(Profile) > 0 &&
                Ids.insert(yyjson_get_str(Id)).second;
            if (!bValid) break;
            Stages.insert(yyjson_get_str(Stage));
        }
    }
    static const std::set<std::string> RequiredStages = {
        "corpus", "import", "cook", "publication", "strict-load",
        "realization", "native", "image", "lifecycle", "timeout",
        "unsupported"};
    bValid = bValid && Stages == RequiredStages && Ids.size() >= 30;
    yyjson_doc_free(Document);
    Record(Result, bValid,
        "production failure catalog has unique cross-stage first-failure categories");
}

} // namespace

FProductionContentTestResult RunProductionContentTests()
{
    FProductionContentTestResult Result;
    TestProfileScaffolding(Result);
    TestOutOfBandMaintainerNote(Result);
    TestDeviceClassRegistryScaffolding(Result);
    TestFailureCatalog(Result);
    return Result;
}
