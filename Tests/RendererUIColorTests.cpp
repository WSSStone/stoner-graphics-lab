#include "Renderer/FUICompositionSettings.h"
#include <iostream>
#include <limits>

int RunRendererUIColorTests()
{
    using namespace Stoner::Renderer;
    int Failed=0;
    const auto Check=[&](bool Value,const char* Name) {
        std::cout<<(Value ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!Value) ++Failed;
    };
    const FOutputTransformSettingsValidator Validator;
    for (const auto& Profile : Validator.GetProfiles())
    {
        FUICompositionSettings Settings;
        Settings.OutputProfileId=Profile.ProfileId;
        Settings.BlendDomain=Profile.DisplayLinearDomain;
        Settings.DisplayGeneration=7;
        const bool EDR=Profile.MetadataPolicy==EOutputMetadataPolicy::EDRState;
        Settings.UIReferenceWhiteNits=Settings.NativePackingWhiteNits=EDR ? 160.0f : Profile.ReferenceWhiteNits;
        Check(Settings.UIWhiteMultiplier==1 && Settings.IsValid(),
            "UI profile accepts default unit multiplier and its resolved reference white");
        for (float Multiplier : {0.25f,2.0f})
        {
            Settings.UIWhiteMultiplier=Multiplier;
            Check(Settings.IsValid(),"UI profile accepts both inclusive brightness limits");
        }
        for (float Multiplier : {0.249f,2.001f,std::numeric_limits<float>::infinity(),
                std::numeric_limits<float>::quiet_NaN()})
        {
            Settings.UIWhiteMultiplier=Multiplier;
            Check(!Settings.IsValid(),"UI profile rejects out-of-range or non-finite brightness");
        }
        Settings.UIWhiteMultiplier=1;
        Settings.NativePackingWhiteNits+=1;
        Check(!Settings.IsValid(),"UI profile rejects inconsistent composition and native packing whites");
        Settings.NativePackingWhiteNits=Settings.UIReferenceWhiteNits;
        Settings.DisplayGeneration=0;
        Check(!Settings.IsValid(),"UI reference white requires a nonzero display generation");
        Settings.DisplayGeneration=7;
        Settings.BlendDomain=ERenderGraphColorDomain::SceneLinearRec709D65;
        Check(!Settings.IsValid(),"UI cannot compose in a scene-linear domain");
    }
    return Failed;
}
