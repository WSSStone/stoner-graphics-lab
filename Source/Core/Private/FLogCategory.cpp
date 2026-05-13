// FLogCategory.cpp — Log category self-registration implementation.
#include "Core/FLogCategory.h"

namespace Stoner::Core
{

// Global category registry. Categories self-register on construction.
static TArray<FLogCategory*>& GetCategoryRegistry()
{
    static TArray<FLogCategory*> Registry;
    return Registry;
}

FLogCategory::FLogCategory(const char* InName, ELogSeverity InDefaultMinSeverity)
    : Name(InName)
    , MinSeverity(InDefaultMinSeverity)
    , DefaultMinSeverity(InDefaultMinSeverity)
{
    GetCategoryRegistry().push_back(this);
}

const TArray<FLogCategory*>& FLogCategory::GetAllCategories()
{
    return GetCategoryRegistry();
}

// Pre-defined log categories for engine layers.
SG_DEFINE_LOG_CATEGORY(LogCore)
SG_DEFINE_LOG_CATEGORY(LogRHI)
SG_DEFINE_LOG_CATEGORY(LogRenderer)
SG_DEFINE_LOG_CATEGORY(LogBackend)
SG_DEFINE_LOG_CATEGORY(LogApplication)

} // namespace Stoner::Core
