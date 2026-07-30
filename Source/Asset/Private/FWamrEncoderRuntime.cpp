#include "FWamrEncoderRuntime.h"

#include "FEmbeddedBasisEncoder.h"
#include "Asset/FAssetDigest.h"
#include "wasm_export.h"

#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{

constexpr Core::uint32 RuntimeStackBytes = 2U * 1024U * 1024U;
constexpr Core::uint32 ErrorBufferBytes = 512;

std::once_flag GRuntimeInitialization;
bool GRuntimeAvailable = false;
std::mutex GRuntimeExecutionMutex;

FAssetDiagnostic MakeDiagnostic(
    const char* Code,
    const char* Field,
    const char* Reason)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Cook;
    Diagnostic.Result = EAssetResult::CookFailure;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString(Code);
    Diagnostic.Participant = Core::FString("encoder.wamr");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(Reason);
    return Diagnostic;
}

bool Call(
    wasm_exec_env_t Environment,
    wasm_module_inst_t Instance,
    const char* Name,
    Core::uint32 ArgumentCount,
    std::vector<Core::uint32>& Cells)
{
    wasm_function_inst_t Function =
        wasm_runtime_lookup_function(Instance, Name);
    return Function != nullptr &&
        wasm_runtime_call_wasm(
            Environment,
            Function,
            ArgumentCount,
        Cells.empty() ? nullptr : Cells.data());
}

class FWamrThreadEnvironment
{
public:
    FWamrThreadEnvironment()
        : bInitialized(wasm_runtime_init_thread_env())
    {
    }

    ~FWamrThreadEnvironment()
    {
        if (bInitialized)
        {
            wasm_runtime_destroy_thread_env();
        }
    }

    [[nodiscard]] bool IsInitialized() const noexcept
    {
        return bInitialized;
    }

private:
    bool bInitialized = false;
};

} // namespace

FWamrEncoderResult FWamrEncoderRuntime::Execute(
    std::span<const Core::uint8> Request,
    Core::uint64 MaxOutputBytes)
{
    FWamrEncoderResult Result;
    const FAssetDigest ModuleDigest = FAssetDigest::FromBytes(
        {GStonerBasisEncoderModule, GStonerBasisEncoderModuleSize});
    if (ModuleDigest.ToLowerHex() !=
        Core::FString(ExpectedModuleSha256))
    {
        Result.Diagnostics.push_back(MakeDiagnostic(
            "asset.ktx2.encoder.module-hash",
            "module",
            "module checksum mismatch"));
        return Result;
    }
    if (Request.empty() ||
        Request.size() > std::numeric_limits<Core::uint32>::max() ||
        MaxOutputBytes == 0 ||
        MaxOutputBytes > std::numeric_limits<Core::uint32>::max())
    {
        Result.Diagnostics.push_back(MakeDiagnostic(
            "asset.ktx2.encoder.request-range",
            "request",
            "request or output budget is outside the module ABI"));
        return Result;
    }

    std::call_once(
        GRuntimeInitialization,
        []
        {
            GRuntimeAvailable = wasm_runtime_init();
        });
    if (!GRuntimeAvailable)
    {
        Result.Diagnostics.push_back(MakeDiagnostic(
            "asset.ktx2.encoder.runtime-init",
            "runtime",
            "interpreter initialization failed"));
        return Result;
    }

    // This WAMR build does not enable its multi-thread runtime. Requests keep
    // independent module instances and memory, while runtime API entry is
    // serialized to preserve deterministic concurrent host requests.
    std::lock_guard RuntimeLock(GRuntimeExecutionMutex);
    FWamrThreadEnvironment ThreadEnvironment;
    if (!ThreadEnvironment.IsInitialized())
    {
        Result.Diagnostics.push_back(MakeDiagnostic(
            "asset.ktx2.encoder.thread-env",
            "runtime",
            "request thread environment initialization failed"));
        return Result;
    }
    Core::TArray<Core::uint8> MutableModule(
        GStonerBasisEncoderModule,
        GStonerBasisEncoderModule + GStonerBasisEncoderModuleSize);
    char Error[ErrorBufferBytes] = {};
    wasm_module_t Module = wasm_runtime_load(
        MutableModule.data(),
        static_cast<Core::uint32>(MutableModule.size()),
        Error,
        sizeof(Error));
    if (Module == nullptr)
    {
        Result.Diagnostics.push_back(MakeDiagnostic(
            "asset.ktx2.encoder.module-load",
            "module",
            "module validation failed"));
        return Result;
    }

    wasm_module_inst_t Instance = wasm_runtime_instantiate(
        Module,
        RuntimeStackBytes,
        0,
        Error,
        sizeof(Error));
    wasm_exec_env_t Environment = Instance == nullptr
        ? nullptr
        : wasm_runtime_create_exec_env(Instance, RuntimeStackBytes);
    if (Instance == nullptr || Environment == nullptr)
    {
        if (Environment != nullptr)
        {
            wasm_runtime_destroy_exec_env(Environment);
        }
        if (Instance != nullptr)
        {
            wasm_runtime_deinstantiate(Instance);
        }
        wasm_runtime_unload(Module);
        Result.Diagnostics.push_back(MakeDiagnostic(
            "asset.ktx2.encoder.instance",
            "instance",
            "request-owned instance allocation failed"));
        return Result;
    }

    Core::uint32 RequestAddress = 0;
    bool Success = true;
    std::vector<Core::uint32> EmptyCells;
    if (!Call(Environment, Instance, "_initialize", 0, EmptyCells))
    {
        Success = false;
    }

    std::vector<Core::uint32> VersionCell(1);
    if (Success &&
        (!Call(
             Environment,
             Instance,
             "stoner_encoder_version",
             0,
             VersionCell) ||
         VersionCell[0] != ExpectedAbiVersion))
    {
        Success = false;
    }

    std::vector<Core::uint32> AllocateCells = {
        static_cast<Core::uint32>(Request.size())};
    if (Success &&
        (!Call(
             Environment,
             Instance,
             "stoner_alloc",
             1,
             AllocateCells) ||
         AllocateCells[0] == 0 ||
         !wasm_runtime_validate_app_addr(
             Instance,
             AllocateCells[0],
             static_cast<Core::uint32>(Request.size()))))
    {
        Success = false;
    }
    RequestAddress = Success ? AllocateCells[0] : 0;
    if (Success)
    {
        std::memcpy(
            wasm_runtime_addr_app_to_native(
                Instance, RequestAddress),
            Request.data(),
            Request.size());
    }

    std::vector<Core::uint32> CookCells = {
        RequestAddress,
        static_cast<Core::uint32>(Request.size())};
    if (Success &&
        (!Call(
             Environment,
             Instance,
             "stoner_cook",
             2,
             CookCells) ||
         CookCells[0] != 0))
    {
        Success = false;
    }

    std::vector<Core::uint32> PointerCell(1);
    std::vector<Core::uint32> SizeCell(1);
    if (Success &&
        (!Call(
             Environment,
             Instance,
             "stoner_result_ptr",
             0,
             PointerCell) ||
         !Call(
             Environment,
             Instance,
             "stoner_result_size",
             0,
             SizeCell) ||
         PointerCell[0] == 0 ||
         SizeCell[0] == 0 ||
         SizeCell[0] > MaxOutputBytes ||
         !wasm_runtime_validate_app_addr(
             Instance, PointerCell[0], SizeCell[0])))
    {
        Success = false;
    }
    if (Success)
    {
        const auto* Output = static_cast<const Core::uint8*>(
            wasm_runtime_addr_app_to_native(
                Instance, PointerCell[0]));
        Result.Bytes.assign(Output, Output + SizeCell[0]);
        Result.Result = EAssetResult::Success;
    }

    (void)Call(
        Environment,
        Instance,
        "stoner_release_result",
        0,
        EmptyCells);
    if (RequestAddress != 0)
    {
        std::vector<Core::uint32> FreeCells = {RequestAddress};
        (void)Call(
            Environment,
            Instance,
            "stoner_free",
            1,
            FreeCells);
    }

    const bool Trapped = wasm_runtime_get_exception(Instance) != nullptr;
    wasm_runtime_destroy_exec_env(Environment);
    wasm_runtime_deinstantiate(Instance);
    wasm_runtime_unload(Module);

    if (!Success)
    {
        Result.Bytes.clear();
        Result.Diagnostics.push_back(MakeDiagnostic(
            Trapped
                ? "asset.ktx2.encoder.trap"
                : "asset.ktx2.encoder.call",
            "module",
            Trapped
                ? "module execution trapped"
                : "module returned an invalid status or range"));
    }
    return Result;
}

} // namespace Stoner::Asset::Private
