// SGPlatformBreak.h — Platform-abstracted debugger break intrinsic.
#pragma once

// SG_DEBUG_BREAK() triggers a platform debugger break in Debug builds.
// In Release builds, it expands to nothing.
// On supported platforms, execution can be resumed from the debugger.

// SCons defines _DEBUG for Debug and NDEBUG for Release. Supporting both
// conventions also keeps standalone compiler and IDE builds predictable.
#if !defined(NDEBUG) || defined(_DEBUG)

    #if defined(_MSC_VER)
        // MSVC: __debugbreak() is always available
        #define SG_DEBUG_BREAK() __debugbreak()
    #elif defined(__clang__)
        // Clang: __builtin_debugtrap() is resumable.
        #define SG_DEBUG_BREAK() __builtin_debugtrap()
    #elif defined(__GNUC__) && (defined(__unix__) || defined(__APPLE__))
        // GCC has no debugtrap intrinsic; SIGTRAP is resumable by a debugger.
        #include <csignal>
        #define SG_DEBUG_BREAK() std::raise(SIGTRAP)
    #else
        // Fallback: abort (not resumable)
        #include <cstdlib>
        #define SG_DEBUG_BREAK() std::abort()
    #endif

#else
    // Release build: no-op
    #define SG_DEBUG_BREAK() ((void)0)
#endif
