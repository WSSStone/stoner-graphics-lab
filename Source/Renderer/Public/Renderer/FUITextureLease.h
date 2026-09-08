#pragma once
#include "Renderer/FUITextureRequest.h"

namespace Stoner::Renderer
{
struct FUITextureRecord;
class FUITextureRegistry;
class FUITextureSubmission;

// A value-copyable, opaque generation owner. Only the registry can mint one;
// snapshots keep it until cancellation or completion of their rendering use.
class FUITextureLease
{
public:
    FUITextureLease() = default;
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] FUITextureId GetId() const noexcept;
private:
    friend class FUITextureRegistry;
    friend class FUITextureSubmission;
    Stoner::Core::TSharedPtr<FUITextureRecord> Record;
};
}
