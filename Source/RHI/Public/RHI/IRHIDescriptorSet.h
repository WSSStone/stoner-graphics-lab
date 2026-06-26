#pragma once

#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

class IRHIBuffer;
class IRHIPipelineLayout;
class IRHISampler;
class IRHITexture;

enum class ERHIDescriptorResourceKind
{
    None,
    Buffer,
    Texture,
    Sampler,
    CombinedTextureSampler
};

class IRHIDescriptorSet
{
public:
    virtual ~IRHIDescriptorSet() = default;

    [[nodiscard]] virtual Stoner::Core::uint32 GetSetIndex() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept = 0;
    [[nodiscard]] virtual ERHIDescriptorResourceKind GetBoundResourceKind(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex = 0) const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetBoundResourceCount() const noexcept = 0;
    [[nodiscard]] virtual ERHIResourceLifecycleState GetLifecycleState() const noexcept = 0;

    virtual ERHIResult UpdateBuffer(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<IRHIBuffer>& Buffer) = 0;
    virtual ERHIResult UpdateTexture(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<IRHITexture>& Texture) = 0;
    virtual ERHIResult UpdateSampler(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<IRHISampler>& Sampler) = 0;
    virtual ERHIResult UpdateCombinedTextureSampler(Stoner::Core::uint32 BindingSlot, Stoner::Core::uint32 ArrayIndex, const Stoner::Core::TSharedPtr<IRHITexture>& Texture, const Stoner::Core::TSharedPtr<IRHISampler>& Sampler) = 0;
    virtual ERHIResult Invalidate() = 0;
};

} // namespace Stoner::RHI
