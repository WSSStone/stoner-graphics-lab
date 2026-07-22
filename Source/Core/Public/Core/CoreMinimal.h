#pragma once

#include "Core/FPlatformTypes.h"
#include "Core/FString.h"
#include "Core/FName.h"
#include "Core/TSharedPtr.h"
#include "Core/TUniquePtr.h"
#include "Core/TArray.h"
#include "Core/TMap.h"
#include "Core/FMemory.h"
#include "Core/FMath.h"
#include "Core/FVector2.h"
#include "Core/FVector3.h"
#include "Core/FVector4.h"
#include "Core/FMatrix4x4.h"
#include "Core/FQuat.h"
#include "Core/FTransform.h"
#include "Core/FColor.h"
#include "Core/FBox.h"
#include "Core/FSphere.h"
#include "Core/FPlane.h"

// Logging & Assertions
#include "Core/ELogSeverity.h"
#include "Core/SGPlatformBreak.h"
#include "Core/FLogCategory.h"
#include "Core/FLogConsoleSink.h"
#include "Core/FLog.h"
#include "Core/SGLog.h"
#include "Core/SGAssert.h"

// Platform abstraction
#include "Core/SGPlatform.h"
#include "Core/FPlatformMisc.h"
#include "Core/FPlatformTime.h"
#include "Core/FPlatformFileSystem.h"
#include "Core/FPlatformProcess.h"
#include "Core/FPlatformMemory.h"
#include "Core/FPlatformWindow.h"

// Core layer minimal header - shared utilities foundation.
namespace Stoner::Core
{
} // namespace Stoner::Core
