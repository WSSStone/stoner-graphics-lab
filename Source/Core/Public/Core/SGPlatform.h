#pragma once

#if defined(_WIN32)
#define SG_PLATFORM_WINDOWS 1
#define SG_PLATFORM_MAC 0
#define SG_PLATFORM_LINUX 0
#elif defined(__ANDROID__)
#error "Unsupported platform for Stoner Graphics Lab: Android"
#elif defined(__APPLE__) && defined(__MACH__)
#include <TargetConditionals.h>
#if defined(TARGET_OS_OSX) && TARGET_OS_OSX
#define SG_PLATFORM_WINDOWS 0
#define SG_PLATFORM_MAC 1
#define SG_PLATFORM_LINUX 0
#else
#error "Unsupported platform for Stoner Graphics Lab: non-macOS Apple target"
#endif
#elif defined(__linux__)
#define SG_PLATFORM_WINDOWS 0
#define SG_PLATFORM_MAC 0
#define SG_PLATFORM_LINUX 1
#else
#error "Unsupported platform for Stoner Graphics Lab"
#endif

#if defined(SG_PLATFORM_WINDOWS) && defined(SG_PLATFORM_MAC) && defined(SG_PLATFORM_LINUX)
#if (SG_PLATFORM_WINDOWS + SG_PLATFORM_MAC + SG_PLATFORM_LINUX) != 1
#error "Exactly one SG_PLATFORM_* macro must be active"
#endif
#endif
