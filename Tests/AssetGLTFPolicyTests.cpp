#include "AssetGLTFPolicyTests.h"

#include "Asset/FStaticModelImport.h"

#include <iostream>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

void Record(FAssetGLTFPolicyTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

} // namespace

FAssetGLTFPolicyTestResult RunAssetGLTFPolicyTests()
{
    FAssetGLTFPolicyTestResult Result;
    FStaticModelImportProfile Profile;
    const FAssetDigest Baseline = Profile.GetDigest();
    Record(Result,
        Profile.Validate() == EAssetResult::Success && Baseline.IsAvailable() &&
            Profile.NormalPolicy == EStaticMeshNormalPolicy::GenerateFlat &&
            Profile.TangentPolicy == EStaticMeshTangentPolicy::GenerateWhenRequired,
        "default static-model policy is explicit and versioned");

    Profile.MaximumTexCoordSets = 3;
    Record(Result,
        Profile.Validate() == EAssetResult::InvalidInput &&
            !Profile.GetDigest().IsAvailable(),
        "profile rejects unsupported UV-set count");

    Profile = {};
    Profile.CoordinateConvention = FString("OtherConvention");
    Record(Result,
        Profile.Validate() == EAssetResult::InvalidInput,
        "profile rejects a non-canonical coordinate convention");

    Profile = {};
    Profile.Limits.MaxScenes = 0;
    Record(Result,
        Profile.Limits.Validate() == EAssetResult::InvalidInput,
        "all static-model limits are finite non-zero values");

    Profile = {};
    Profile.Limits.MaxSingleDependencyBytes =
        Profile.Limits.MaxAggregateDependencyBytes + 1;
    Record(Result,
        Profile.Limits.Validate() == EAssetResult::InvalidInput,
        "single dependency limit cannot exceed aggregate limit");

    Profile = {};
    Profile.NormalPolicy = EStaticMeshNormalPolicy::RequireSource;
    const FAssetDigest StrictNormal = Profile.GetDigest();
    Profile = {};
    Profile.Limits.MaxPrimitives -= 1;
    const FAssetDigest ChangedLimit = Profile.GetDigest();
    Record(Result,
        StrictNormal.IsAvailable() && ChangedLimit.IsAvailable() &&
            StrictNormal != Baseline && ChangedLimit != Baseline,
        "policy and limit changes invalidate import version evidence");
    return Result;
}
