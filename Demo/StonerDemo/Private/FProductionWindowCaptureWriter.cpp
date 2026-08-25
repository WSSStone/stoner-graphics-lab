#include "FProductionWindowCaptureWriter.h"

#include "Asset/FAssetDigest.h"
#include "FDemoBackendFactory.h"
#include "FStonerDemoApplication.h"
#include "FStonerDemoWindowState.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>

namespace Stoner::Demo
{
namespace
{

bool IsToken(const char* Value)
{
    if (!Value || *Value == '\0') return false;
    for (const char* Cursor = Value; *Cursor != '\0'; ++Cursor)
    {
        const char Character = *Cursor;
        if (!((Character >= 'a' && Character <= 'z') ||
              (Character >= '0' && Character <= '9') ||
              Character == '.' || Character == '-'))
            return false;
    }
    return true;
}

std::string CaptureStem(Core::uint32 Cycle)
{
    std::ostringstream Stream;
    Stream << "capture-" << std::setw(2) << std::setfill('0') << Cycle - 1u;
    return Stream.str();
}

} // namespace

void FStonerDemoApplication::RecordProductionCapture(
    FDemoProductionCapture Capture)
{
    ++ProductionExecutionInspection.CaptureCount;
    if (Capture.Name == Core::FString("FinalOutput"))
    {
        ++ProductionExecutionInspection.FinalOutputCaptureCount;
        if (Capture.bPresented && Capture.bWindowOnlyCapture)
            ++ProductionExecutionInspection.PresentedFinalOutputCaptureCount;
    }
    else if (Capture.Name == Core::FString("ForwardColor"))
        ++ProductionExecutionInspection.ForwardColorCaptureCount;
    if (!Capture.Bytes.empty())
        ProductionExecutionInspection.Captures.push_back(std::move(Capture));
}

bool FStonerDemoApplication::PresentProductionCaptureWithRecovery(
    FDemoProductionCapture& Capture)
{
    if (!Window || !BackendRuntime) return false;
    constexpr auto RecoveryLimit = std::chrono::milliseconds(2000);
    const auto Started = std::chrono::steady_clock::now();
    RHI::ERHIResult Result = RHI::ERHIResult::Unavailable;
    FDemoProductionPresentationResult Presented;
    do
    {
        (void)Window->Value.PollEvents();
        if (Window->Value.IsCloseRequested()) return false;
        Presented = {};
        Result = BackendRuntime->PresentProductionImage(
            Capture.Bytes, Capture.Width, Capture.Height,
            Capture.RowPitchBytes, Presented);
        if (Result == RHI::ERHIResult::Success) break;
        const bool bRecoverable = Result == RHI::ERHIResult::ResizeRequired ||
            (Configuration.GraphicsBackend == EDemoGraphicsBackend::Metal &&
                (Result == RHI::ERHIResult::Unavailable ||
                    Result == RHI::ERHIResult::NotReady));
        if (!bRecoverable) break;
        if (Result == RHI::ERHIResult::ResizeRequired)
        {
            const Core::uint32 Width = Window->Value.GetDrawableWidth();
            const Core::uint32 Height = Window->Value.GetDrawableHeight();
            if (Width != 0 && Height != 0)
                Result = BackendRuntime->RecreatePresentation(Width, Height);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    while (std::chrono::steady_clock::now() - Started <= RecoveryLimit);

    if (Result != RHI::ERHIResult::Success || !Presented.bPresented ||
        Presented.Rgba8.empty())
    {
        const Core::FString Reason(
            "production presentation recovery failed; result=" +
            std::to_string(static_cast<int>(Result)));
        Diagnostics.Add(EDemoStage::Present, EDemoExitCode::ValidationFailed,
            "ProductionPresentation", Reason.CStr());
        return false;
    }
    if (Presented.Width == Capture.Width &&
        Presented.Height == Capture.Height &&
        Presented.RowPitchBytes == Capture.RowPitchBytes &&
        Presented.Rgba8 != Capture.Bytes)
    {
        Diagnostics.Add(EDemoStage::Readback,
            EDemoExitCode::ValidationFailed, "ProductionPresentation",
            "native presentation readback differs from the submitted RGBA image");
        return false;
    }
    Capture.Width = Presented.Width;
    Capture.Height = Presented.Height;
    Capture.RowPitchBytes = Presented.RowPitchBytes;
    Capture.Format = RHI::ERHIFormat::R8G8B8A8_UNorm;
    Capture.bPresented = true;
    Capture.bWindowOnlyCapture = true;
    Capture.Bytes = std::move(Presented.Rgba8);
    return true;
}

bool WriteProductionWindowCapture(
    const FDemoProductionCapture& Capture,
    const char* Backend,
    const char* WorkloadRevision,
    const char* Root)
{
    if (!IsToken(Backend) || !IsToken(WorkloadRevision) ||
        !Root || *Root == '\0' || Capture.Cycle == 0 ||
        Capture.Cycle > 20 || Capture.Name != Core::FString("FinalOutput") ||
        !Capture.bPresented || !Capture.bWindowOnlyCapture ||
        Capture.Bytes.empty() || Capture.Width == 0 || Capture.Height == 0 ||
        Capture.RowPitchBytes < Capture.Width * 4u)
        return false;

    std::error_code Error;
    const std::filesystem::path Output =
        std::filesystem::path(Root) / Backend;
    if (Capture.Cycle == 1)
        std::filesystem::remove_all(Output, Error);
    if (Error) return false;
    std::filesystem::create_directories(Output, Error);
    if (Error) return false;

    const std::string Stem = CaptureStem(Capture.Cycle);
    const auto PpmPath = Output / (Stem + ".ppm");
    const auto JsonPath = Output / (Stem + ".json");
    const std::string Header = "P6\n" + std::to_string(Capture.Width) +
        " " + std::to_string(Capture.Height) + "\n255\n";
    Core::TArray<Core::uint8> Ppm(Header.begin(), Header.end());
    Ppm.reserve(Ppm.size() +
        static_cast<Core::usize>(Capture.Width) * Capture.Height * 3u);
    for (Core::uint32 Y = 0; Y < Capture.Height; ++Y)
    {
        const Core::usize Row =
            static_cast<Core::usize>(Y) * Capture.RowPitchBytes;
        for (Core::uint32 X = 0; X < Capture.Width; ++X)
        {
            const Core::usize Pixel = Row + static_cast<Core::usize>(X) * 4u;
            Ppm.push_back(Capture.Bytes[Pixel]);
            Ppm.push_back(Capture.Bytes[Pixel + 1u]);
            Ppm.push_back(Capture.Bytes[Pixel + 2u]);
        }
    }
    std::ofstream Image(PpmPath, std::ios::binary | std::ios::trunc);
    if (!Image) return false;
    Image.write(reinterpret_cast<const char*>(Ppm.data()),
        static_cast<std::streamsize>(Ppm.size()));
    Image.close();
    if (!Image) return false;

    const Core::FString Digest =
        Asset::FAssetDigest::FromBytes(Ppm).ToLowerHex();
    const std::string FrameToken =
        "cycle-" + std::to_string(Capture.Cycle);
    std::ofstream Metadata(JsonPath, std::ios::binary | std::ios::trunc);
    if (!Metadata) return false;
    Metadata
        << "{\n"
        << "  \"backend\": \"" << Backend << "\",\n"
        << "  \"captureScope\": \"application-window\",\n"
        << "  \"captureStartedNs\": " << Capture.CaptureStartedNs << ",\n"
        << "  \"expectedFrameToken\": \"" << FrameToken << "\",\n"
        << "  \"frameToken\": \"" << FrameToken << "\",\n"
        << "  \"height\": " << Capture.Height << ",\n"
        << "  \"sha256\": \"" << Digest.CStr() << "\",\n"
        << "  \"width\": " << Capture.Width << ",\n"
        << "  \"workloadRevision\": \"" << WorkloadRevision << "\"\n"
        << "}\n";
    Metadata.close();
    return Metadata.good();
}

bool ValidateProductionWindowCaptureSet(
    const char* Backend,
    const char* Root,
    Core::uint32 ExpectedCount)
{
    if (!IsToken(Backend) || !Root || *Root == '\0' || ExpectedCount == 0)
        return false;
    const std::filesystem::path Output =
        std::filesystem::path(Root) / Backend;
    std::error_code Error;
    Core::uint32 RegularFiles = 0;
    for (const auto& Entry : std::filesystem::directory_iterator(Output, Error))
    {
        if (Error || !Entry.is_regular_file()) return false;
        ++RegularFiles;
    }
    if (Error || RegularFiles != ExpectedCount * 2u) return false;
    for (Core::uint32 Cycle = 1; Cycle <= ExpectedCount; ++Cycle)
    {
        const std::string Stem = CaptureStem(Cycle);
        if (!std::filesystem::is_regular_file(Output / (Stem + ".ppm"), Error) ||
            Error || !std::filesystem::is_regular_file(
                Output / (Stem + ".json"), Error) || Error)
            return false;
    }
    return true;
}

} // namespace Stoner::Demo
