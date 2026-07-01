#pragma once

#include "Renderer/FRenderGraphDiagnostics.h"
#include "Renderer/FRenderGraphResource.h"

#include <functional>
#include <string>

namespace Stoner::Renderer
{

class FRenderGraphExecutionContext;

struct FRenderGraphPassHandle
{
    Stoner::Core::uint32 GraphId = 0;
    Stoner::Core::uint32 Index = InvalidIndex;

    static constexpr Stoner::Core::uint32 InvalidIndex = 0xFFFFFFFFu;

    [[nodiscard]] bool IsValid() const noexcept { return GraphId != 0 && Index != InvalidIndex; }
    [[nodiscard]] friend bool operator==(FRenderGraphPassHandle Left, FRenderGraphPassHandle Right) noexcept
    {
        return Left.GraphId == Right.GraphId && Left.Index == Right.Index;
    }
};

enum class ERenderGraphPassType
{
    Graphics,
    Compute,
    Copy,
    SideEffect
};

enum class ERenderGraphAccessType
{
    Read,
    Write,
    ReadWrite,
    Create,
    Import,
    Export,
    Preserve
};

struct FRenderGraphResourceAccess
{
    FRenderGraphResourceHandle Resource;
    ERenderGraphAccessType Access = ERenderGraphAccessType::Read;
    ERenderGraphResourceState RequiredState = ERenderGraphResourceState::Read;
};

using FRenderGraphPassCallback = std::function<ERenderGraphResult(FRenderGraphExecutionContext&)>;

struct FRenderGraphPassDesc
{
    std::string Name;
    ERenderGraphPassType Type = ERenderGraphPassType::Graphics;
    bool bPreserveForSideEffects = false;
    Stoner::Core::TArray<FRenderGraphResourceAccess> Accesses;
    FRenderGraphPassCallback Callback;

    [[nodiscard]] static FRenderGraphPassDesc Make(std::string InName, ERenderGraphPassType InType);
};

struct FRenderGraphPassRecord
{
    FRenderGraphPassHandle Handle;
    FRenderGraphPassDesc Desc;
};

[[nodiscard]] bool ReadsResource(ERenderGraphAccessType Access) noexcept;
[[nodiscard]] bool WritesResource(ERenderGraphAccessType Access) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphPassType Type) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphAccessType Access) noexcept;

} // namespace Stoner::Renderer
