#import <Metal/Metal.h>

namespace Stoner::Backend::Metal
{

bool HasSystemDefaultMetalDevice()
{
    @autoreleasepool
    {
        return MTLCreateSystemDefaultDevice() != nil;
    }
}

} // namespace Stoner::Backend::Metal
