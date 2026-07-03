#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardDiagnostics.h"

namespace Stoner::Renderer
{

struct FForwardFramePlan;

enum class EForwardGraphAccess
{
    Read,
    Write,
    ReadWrite,
    Output
};

struct FForwardPassDeclaration
{
    Stoner::Core::FString Name;
    Stoner::Core::FString StageName;
    Stoner::Core::uint32 DrawCount = 0;
};

struct FForwardResourceDeclaration
{
    Stoner::Core::FString Name;
    Stoner::Core::FString Kind;
    Stoner::Core::FString FormatSummary;
};

struct FForwardAccessDeclaration
{
    Stoner::Core::FString PassName;
    Stoner::Core::FString ResourceName;
    EForwardGraphAccess Access = EForwardGraphAccess::Read;
};

struct FForwardGraphOutputSummary
{
    Stoner::Core::FString ColorTargetName;
    Stoner::Core::FString DepthTargetName;
};

class FForwardRenderGraphDeclaration
{
public:
    void Clear();
    void AddPass(FForwardPassDeclaration Pass);
    void AddResource(FForwardResourceDeclaration Resource);
    void AddAccess(FForwardAccessDeclaration Access);
    void AddOutput(FForwardGraphOutputSummary Output);

    [[nodiscard]] const Stoner::Core::TArray<FForwardPassDeclaration>& GetPasses() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FForwardResourceDeclaration>& GetResources() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FForwardAccessDeclaration>& GetAccesses() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<FForwardGraphOutputSummary>& GetOutputs() const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    Stoner::Core::TArray<FForwardPassDeclaration> Passes;
    Stoner::Core::TArray<FForwardResourceDeclaration> Resources;
    Stoner::Core::TArray<FForwardAccessDeclaration> Accesses;
    Stoner::Core::TArray<FForwardGraphOutputSummary> Outputs;
};

[[nodiscard]] FForwardRenderGraphDeclaration BuildForwardRenderGraphDeclaration(const FForwardFramePlan& Plan,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] const char* ToString(EForwardGraphAccess Access) noexcept;

} // namespace Stoner::Renderer
