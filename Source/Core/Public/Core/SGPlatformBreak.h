// SGPlatformBreak.h — Platform-abstracted debugger break intrinsic.
#pragma once

// SG_DEBUG_BREAK() triggers a platform debugger break in Debug builds.
// In Release builds, it expands to nothing.
// On supported platforms, execution can be resumed from the debugger.

#ifdef _DEBUG

    #if defined(_MSC_VER)
        // MSVC: __debugbreak() is always available
        #define SG_DEBUG_BREAK() __debugbreak()
    #elif defined(__clang__) || defined(__GNUC__)
        // GCC/Clang: __builtin_debugtrap() is resumable
        #define SG_DEBUG_BREAK() __builtin_debugtrap()
    #else
        // Fallback: abort (not resumable)
        #include <cstdlib>
        #define SG_DEBUG_BREAK() std::abort()
    #endif

#else
    // Release build: no-op
    #define SG_DEBUG_BREAK() ((void)0)
#endif
