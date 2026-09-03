#include "Renderer/FRenderGraphPass.h"

#include <utility>

namespace Stoner::Renderer
{

FRenderGraphPassDesc FRenderGraphPassDesc::Make(Stoner::Core::FString InName, ERenderGraphPassType InType)
{
    FRenderGraphPassDesc Desc;
    Desc.Name = std::move(InName);
    Desc.Type = InType;
    Desc.bPreserveForSideEffects = InType == ERenderGraphPassType::SideEffect;
    return Desc;
}

bool ReadsResource(ERenderGraphAccessType Access) noexcept
{
    return Access == ERenderGraphAccessType::Read || Access == ERenderGraphAccessType::ReadWrite ||
        Access == ERenderGraphAccessType::Export || Access == ERenderGraphAccessType::Preserve;
}

bool WritesResource(ERenderGraphAccessType Access) noexcept
{
    return Access == ERenderGraphAccessType::Write || Access == ERenderGraphAccessType::ReadWrite ||
        Access == ERenderGraphAccessType::Create || Access == ERenderGraphAccessType::Import;
}

const char* ToString(ERenderGraphPassType Type) noexcept
{
    switch (Type)
    {
    case ERenderGraphPassType::Graphics: return "Graphics";
    case ERenderGraphPassType::Compute: return "Compute";
    case ERenderGraphPassType::Copy: return "Copy";
    case ERenderGraphPassType::SideEffect: return "SideEffect";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphAccessType Access) noexcept
{
    switch (Access)
    {
    case ERenderGraphAccessType::Read: return "Read";
    case ERenderGraphAccessType::Write: return "Write";
    case ERenderGraphAccessType::ReadWrite: return "ReadWrite";
    case ERenderGraphAccessType::Create: return "Create";
    case ERenderGraphAccessType::Import: return "Import";
    case ERenderGraphAccessType::Export: return "Export";
    case ERenderGraphAccessType::Preserve: return "Preserve";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphExternalSideEffect SideEffect) noexcept
{
    switch (SideEffect)
    {
    case ERenderGraphExternalSideEffect::None: return "None";
    case ERenderGraphExternalSideEffect::Readback: return "Readback";
    case ERenderGraphExternalSideEffect::Presentation: return "Presentation";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
