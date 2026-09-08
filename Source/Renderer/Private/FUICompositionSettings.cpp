#include "Renderer/FUICompositionSettings.h"

#include <cmath>

namespace Stoner::Renderer
{

bool FUICompositionSettings::IsValid() const noexcept
{
    if (DisplayGeneration == 0 || OutputProfileId.IsEmpty() ||
        !std::isfinite(UIWhiteMultiplier) || UIWhiteMultiplier < 0.25f ||
        UIWhiteMultiplier > 2.0f ||
        !std::isfinite(UIReferenceWhiteNits) ||
        !std::isfinite(NativePackingWhiteNits) ||
        UIReferenceWhiteNits <= 0.0f || NativePackingWhiteNits <= 0.0f)
    {
        return false;
    }

    const FOutputTransformSettingsValidator Validator;
    const FOutputDeviceProfile* Profile =
        Validator.FindProfile(OutputProfileId);
    if (Profile == nullptr || BlendDomain != Profile->DisplayLinearDomain)
    {
        return false;
    }

    switch (Profile->DynamicRange)
    {
    case EOutputDynamicRange::SDR:
        // SDR UI is normalized display-linear Rec.709 in the shader, while
        // these copied fields retain the profile's physical reference white.
        return UIReferenceWhiteNits == Profile->ReferenceWhiteNits &&
            NativePackingWhiteNits == Profile->ReferenceWhiteNits;
    case EOutputDynamicRange::HDR:
        if (Profile->MetadataPolicy == EOutputMetadataPolicy::HDR10Static)
        {
            // PQ's reference white is the frozen 100-nit UI contribution.
            return UIReferenceWhiteNits == Profile->ReferenceWhiteNits &&
                NativePackingWhiteNits == Profile->ReferenceWhiteNits;
        }
        if (Profile->MetadataPolicy == EOutputMetadataPolicy::EDRState)
        {
            // EDR resolves its white per display generation. Both values must
            // be the same captured white; a hard-coded 100-nit assumption is
            // intentionally not accepted here.
            return UIReferenceWhiteNits == NativePackingWhiteNits;
        }
        return false;
    }
    return false;
}

} // namespace Stoner::Renderer
