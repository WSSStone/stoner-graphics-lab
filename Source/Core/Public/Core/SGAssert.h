// SGAssert.h — Assertion macros with Debug/Release behavior differentiation.
#pragma once

#include "Core/FLog.h"

// SG_CHECK(Expr)
// Debug build: evaluates Expr, on failure logs file/line/expression and triggers assertion handler.
// Release build: entire macro is compiled out. Zero cost.
//
// SG_VERIFY(Expr)
// All builds: always evaluates Expr.
// Debug build: if Expr is falsy, logs and triggers assertion handler.
// Release build: Expr is evaluated but result is discarded.
//
// SG_CHECKF(Expr, Format, ...)
// Same as SG_CHECK but includes a printf-formatted failure message.

// SCons defines _DEBUG for Debug and NDEBUG for Release. Supporting both
// conventions also keeps standalone compiler and IDE builds predictable.
#if !defined(NDEBUG) || defined(_DEBUG)

#define SG_CHECK(Expr) \
    do { \
        if (!(Expr)) \
        { \
            ::Stoner::Core::FLog::HandleAssertionFailure( \
                __FILE__, __LINE__, #Expr); \
        } \
    } while (0)

#define SG_VERIFY(Expr) \
    do { \
        if (!(Expr)) \
        { \
            ::Stoner::Core::FLog::HandleAssertionFailure( \
                __FILE__, __LINE__, #Expr); \
        } \
    } while (0)

#define SG_CHECKF(Expr, Format, ...) \
    do { \
        if (!(Expr)) \
        { \
            ::Stoner::Core::FLog::HandleAssertionFailure( \
                __FILE__, __LINE__, #Expr, Format, ##__VA_ARGS__); \
        } \
    } while (0)

#else // Release build

#define SG_CHECK(Expr) ((void)0)

#define SG_VERIFY(Expr) \
    do { (void)(Expr); } while (0)

#define SG_CHECKF(Expr, Format, ...) ((void)0)

#endif
