#include "VulkanRHI/FVulkanPhysicalDevice.h"

using namespace Stoner::Backend::Vulkan;

namespace
{

FVulkanAdapterCandidate MakeCandidate(const char* Name)
{
    return {
        Name,
        EVulkanPhysicalDeviceType::Discrete,
        true,
        {true, true, true, false},
        false,
        {true, true},
        0,
        "",
    };
}

} // namespace

int main()
{
    const FVulkanAdapterSelection Selection =
        SelectBestAdapter({MakeCandidate(nullptr), MakeCandidate("Named")});
    return Selection.bSucceeded ? 0 : 3;
}
