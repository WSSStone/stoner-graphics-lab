#include "AssetGLTFMalformedTests.h"

#include "StaticModelTestSupport.h"

#include <iostream>
#include <string>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetGLTFMalformedTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

std::string BaseText()
{
    const auto Bytes = ReadFixture(
        "Tests/Fixtures/StaticModel/Valid/Geometry/01-basis-u16.gltf");
    return {Bytes.begin(), Bytes.end()};
}

bool Rejects(std::string Text)
{
    EAssetResult ImportResult = EAssetResult::Success;
    const TArray<uint8> Bytes(Text.begin(), Text.end());
    const auto Outputs = Import(
        MakeMemoryRequest(Bytes, FString("Hardening/mutation.gltf")), ImportResult);
    const bool Rejected =
        ImportResult != EAssetResult::Success && Outputs.empty();
    if (!Rejected)
        std::cout << "[DETAIL] mutation result=" << static_cast<int>(ImportResult)
                  << " outputs=" << Outputs.size() << '\n';
    return Rejected;
}

bool ReplaceOnce(std::string& Text, const std::string& Before,
    const std::string& After)
{
    const std::size_t At = Text.find(Before);
    if (At == std::string::npos) return false;
    Text.replace(At, Before.size(), After);
    return true;
}
}

FAssetGLTFMalformedTestResult RunAssetGLTFMalformedTests()
{
    FAssetGLTFMalformedTestResult Result;
    std::string Truncated = BaseText();
    Truncated.resize(Truncated.size() / 2);
    Record(Result, Rejects(std::move(Truncated)),
        "truncated JSON rejects without package publication");

    std::string Topology = BaseText();
    const bool TopologyMutated = ReplaceOnce(
        Topology, "\"mode\":4", "\"mode\":1");
    Record(Result, TopologyMutated && Rejects(std::move(Topology)),
        "non-triangle topology fails closed");

    std::string RequiredExtension = BaseText();
    const bool ExtensionMutated = ReplaceOnce(RequiredExtension,
        "\"asset\":", "\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],\"asset\":");
    Record(Result, ExtensionMutated && Rejects(std::move(RequiredExtension)),
        "unsupported required extension fails closed");

    std::string Morph = BaseText();
    const bool MorphMutated = ReplaceOnce(Morph,
        "\"attributes\":", "\"targets\":[{\"POSITION\":0}],\"attributes\":");
    Record(Result, MorphMutated && Rejects(std::move(Morph)),
        "morph target package fails closed");

    std::string Skin = BaseText();
    const bool SkinArray = ReplaceOnce(
        Skin, "\"scenes\":", "\"skins\":[{\"joints\":[0]}],\"scenes\":");
    const bool SkinNode = ReplaceOnce(
        Skin, "\"nodes\":[{", "\"nodes\":[{\"skin\":0,");
    Record(Result, SkinArray && SkinNode && Rejects(std::move(Skin)),
        "skeletal package fails closed");

    std::string Semantic = BaseText();
    const bool SemanticMutated = ReplaceOnce(
        Semantic, "TEXCOORD_0", "COLOR_0");
    Record(Result, SemanticMutated && Rejects(std::move(Semantic)),
        "unsupported vertex semantic fails closed");
    return Result;
}
