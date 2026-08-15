#pragma once

#include "Asset/FPublishedGenerationValidator.h"

namespace Stoner::AssetCooker::Private
{

using EPublishedValidationSubject = Asset::EPublishedValidationSubject;
using EPublishedCorruptionCategory = Asset::EPublishedCorruptionCategory;
using FPublishedGenerationValidationRequest =
    Asset::FPublishedGenerationValidationRequest;
using FPublishedGenerationValidationResult =
    Asset::FPublishedGenerationValidationResult;
using FPublishedGenerationValidator = Asset::FPublishedGenerationValidator;

} // namespace Stoner::AssetCooker::Private
