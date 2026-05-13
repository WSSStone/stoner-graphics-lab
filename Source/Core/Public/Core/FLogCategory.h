// FLogCategory.h — Named log category with severity filtering and self-registration.
#pragma once

#include "Core/ELogSeverity.h"
#include "Core/TArray.h"

namespace Stoner::Core
{

// Forward declaration for the global category list.
struct FLogCategory;

// Macro to declare an extern log category in a header (cross-TU).
// Usage: SG_DECLARE_LOG_CATEGORY_EXTERN(LogCore, ELogSeverity::Verbose)
#define SG_DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultSeverity) \
    extern ::Stoner::Core::FLogCategory CategoryName;

// Macro to define a log category in a source file.
// Usage: SG_DEFINE_LOG_CATEGORY(LogCore)
#define SG_DEFINE_LOG_CATEGORY(CategoryName) \
    ::Stoner::Core::FLogCategory CategoryName(#CategoryName, DefaultSeverity_##CategoryName);

// Helper macro to pair declaration default severity with definition.
// The EXTERN macro stores the default severity in a constexpr for the DEFINE macro.
#undef SG_DECLARE_LOG_CATEGORY_EXTERN
#undef SG_DEFINE_LOG_CATEGORY

#define SG_DECLARE_LOG_CATEGORY_EXTERN(CategoryName, DefaultSeverity) \
    inline constexpr ::Stoner::Core::ELogSeverity DefaultSeverity_##CategoryName = DefaultSeverity; \
    extern ::Stoner::Core::FLogCategory CategoryName;

#define SG_DEFINE_LOG_CATEGORY(CategoryName) \
    ::Stoner::Core::FLogCategory CategoryName(#CategoryName, DefaultSeverity_##CategoryName);

// Named log category with per-category severity filtering.
// Categories self-register into a global list on construction.
struct FLogCategory
{
    // Construct a category with a name and default minimum severity.
    FLogCategory(const char* InName, ELogSeverity InDefaultMinSeverity);

    // Non-copyable, non-movable (static lifetime objects).
    FLogCategory(const FLogCategory&) = delete;
    FLogCategory& operator=(const FLogCategory&) = delete;

    // Get the human-readable category name.
    [[nodiscard]] const char* GetName() const { return Name; }

    // Get the current minimum severity threshold.
    [[nodiscard]] ELogSeverity GetMinSeverity() const { return MinSeverity; }

    // Update the per-category severity threshold at runtime.
    void SetMinSeverity(ELogSeverity NewSeverity) { MinSeverity = NewSeverity; }

    // Get the immutable default severity set at declaration time.
    [[nodiscard]] ELogSeverity GetDefaultMinSeverity() const { return DefaultMinSeverity; }

    // Get all registered categories (for runtime enumeration).
    [[nodiscard]] static const TArray<FLogCategory*>& GetAllCategories();

private:
    const char* Name;
    ELogSeverity MinSeverity;
    ELogSeverity DefaultMinSeverity;
};

// Pre-defined log categories for engine layers.
SG_DECLARE_LOG_CATEGORY_EXTERN(LogCore, ELogSeverity::Verbose)
SG_DECLARE_LOG_CATEGORY_EXTERN(LogRHI, ELogSeverity::Verbose)
SG_DECLARE_LOG_CATEGORY_EXTERN(LogRenderer, ELogSeverity::Verbose)
SG_DECLARE_LOG_CATEGORY_EXTERN(LogBackend, ELogSeverity::Verbose)
SG_DECLARE_LOG_CATEGORY_EXTERN(LogApplication, ELogSeverity::Verbose)

} // namespace Stoner::Core
