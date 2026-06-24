#pragma once

#if defined(_WIN32)
#define SG_PLATFORM_WINDOWS 1
#define SG_PLATFORM_MAC 0
#define SG_PLATFORM_LINUX 0
#elif defined(__APPLE__) && defined(__MACH__)
#define SG_PLATFORM_WINDOWS 0
#define SG_PLATFORM_MAC 1
#define SG_PLATFORM_LINUX 0
#elif defined(__linux__)
#define SG_PLATFORM_WINDOWS 0
#define SG_PLATFORM_MAC 0
#define SG_PLATFORM_LINUX 1
#else
#error "Unsupported platform for Stoner Graphics Lab"
#endif

#if (SG_PLATFORM_WINDOWS + SG_PLATFORM_MAC + SG_PLATFORM_LINUX) != 1
#error "Exactly one SG_PLATFORM_* macro must be active"
#endif
