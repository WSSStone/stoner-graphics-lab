#include "FLabCaptureExport.h"
#include "Asset/FAssetDigest.h"
#include "Core/FUnicode.h"
#include <algorithm>
#include <sstream>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>

namespace Stoner::Demo
{
namespace
{
using namespace Core;
void BE(TArray<uint8>& Out,uint32 V)
{ for (int S=24;S>=0;S-=8) Out.push_back(static_cast<uint8>(V>>S)); }
uint32 CRC(std::span<const uint8> Bytes)
{
    uint32 C=~0u;
    for (auto B : Bytes) { C^=B; for (unsigned I=0;I<8;++I) C=(C>>1)^((C&1) ? 0xedb88320u : 0u); }
    return ~C;
}
void Chunk(TArray<uint8>& Out,const char* Tag,const TArray<uint8>& Data)
{
    BE(Out,static_cast<uint32>(Data.size())); const auto Start=Out.size();
    Out.insert(Out.end(),Tag,Tag+4); Out.insert(Out.end(),Data.begin(),Data.end());
    BE(Out,CRC(std::span<const uint8>(Out).subspan(Start)));
}
std::string Quote(const FString& Text)
{
    std::string Out="\"";
    constexpr char Hex[]="0123456789abcdef";
    for (const unsigned char C : Text.ToStdString())
    {
        if (C=='"' || C=='\\') { Out+='\\'; Out+=static_cast<char>(C); }
        else if (C<32) { Out+="\\u00"; Out+=Hex[C>>4]; Out+=Hex[C&15]; }
        else Out+=static_cast<char>(C);
    }
    return Out+'"';
}
}
bool EncodeLabCapture(const FLabCaptureCompletion& Completion,const Core::TArray<Core::uint8>& Bytes,
    const Core::FString& SoftwareRevision,FLabCaptureEncoded& Out)
{
    using namespace Core; using namespace RHI;
    Out={}; const auto& I=Completion.Request.Target;
    FRHITextureBufferCopyRegion Region; Region.Width=I.Width; Region.Height=I.Height;
    uint64 Size=0;
    if (Completion.Status!=ELabCaptureStatus::Success || !Completion.Request.RequestId || !Completion.FrameToken ||
        !I.IsValid() || SoftwareRevision.IsEmpty() || SoftwareRevision.Len()>256 ||
        !TryGetRHITextureBufferCopyByteSize(Region,I.Format,Size) || Size!=Bytes.size() || Size>64ull*1024*1024)
        return false;
    FString CheckedText;
    if (FUnicode::NormalizeNFC(I.Stage,CheckedText)!=EUnicodeResult::Success ||
        FUnicode::NormalizeNFC(SoftwareRevision,CheckedText)!=EUnicodeResult::Success) return false;
    const auto* Profile=Renderer::FOutputTransformSettingsValidator().FindProfile(I.OutputProfile);
    if (!Profile || (I.Purpose==ELabCapturePurpose::SDRPreview && Profile->DynamicRange!=Renderer::EOutputDynamicRange::SDR)) return false;
    FLabCaptureEncoded Result;
    Result.bPNG=I.Purpose==ELabCapturePurpose::SDRPreview;
    if (Result.bPNG)
    {
        if (I.Stage!=FString("FinalOutput") ||
            (I.Format!=ERHIFormat::R8G8B8A8_UNorm && I.Format!=ERHIFormat::R8G8B8A8_sRGB && I.Format!=ERHIFormat::B8G8R8A8_UNorm))
            return false;
        TArray<uint8> Scanlines; Scanlines.reserve(Bytes.size()+I.Height);
        for (uint32 Y=0;Y<I.Height;++Y)
        {
            Scanlines.push_back(0);
            for (uint32 X=0;X<I.Width;++X)
            {
                const auto P=(static_cast<usize>(Y)*I.Width+X)*4;
                const bool BGRA=I.Format==ERHIFormat::B8G8R8A8_UNorm;
                Scanlines.insert(Scanlines.end(),{Bytes[P+(BGRA ? 2 : 0)],Bytes[P+1],Bytes[P+(BGRA ? 0 : 2)],Bytes[P+3]});
            }
        }
        TArray<uint8> Deflate{0x78,0x01};
        for (usize P=0;P<Scanlines.size();)
        {
            const auto N=static_cast<uint16>(std::min<usize>(65535,Scanlines.size()-P));
            Deflate.push_back(P+N==Scanlines.size() ? 1 : 0);
            Deflate.insert(Deflate.end(),{static_cast<uint8>(N),static_cast<uint8>(N>>8),
                static_cast<uint8>(~N),static_cast<uint8>((~N)>>8)});
            Deflate.insert(Deflate.end(),Scanlines.begin()+P,Scanlines.begin()+P+N); P+=N;
        }
        uint32 A=1,B=0;
        for (auto V : Scanlines) { A=(A+V)%65521; B=(B+A)%65521; }
        BE(Deflate,(B<<16)|A);
        Result.Payload={137,80,78,71,13,10,26,10};
        TArray<uint8> Header; BE(Header,I.Width); BE(Header,I.Height); Header.insert(Header.end(),{8,6,0,0,0});
        Chunk(Result.Payload,"IHDR",Header); Chunk(Result.Payload,"IDAT",Deflate); Chunk(Result.Payload,"IEND",{});
    }
    else Result.Payload=Bytes;
    double Minimum=std::numeric_limits<double>::infinity(),Maximum=-Minimum;
    uint64 Finite=0,NonFinite=0;
    const auto Sample=[&](double V) {
        if (!std::isfinite(V)) { ++NonFinite; return; }
        ++Finite; Minimum=std::min(Minimum,V); Maximum=std::max(Maximum,V);
    };
    if (!Result.bPNG)
    {
        if (I.Format==ERHIFormat::R16G16B16A16_Float)
            for (usize P=0;P<Bytes.size();P+=2)
            {
                const uint16 H=static_cast<uint16>(Bytes[P]|(static_cast<uint16>(Bytes[P+1])<<8));
                const unsigned E=(H>>10)&31, M=H&1023;
                Sample(E==31 ? std::numeric_limits<double>::quiet_NaN() :
                    std::ldexp(static_cast<double>(E ? 1024+M : M),E ? static_cast<int>(E)-25 : -24)*(H&0x8000 ? -1.0 : 1.0));
            }
        else if (I.Format==ERHIFormat::R10G10B10A2_UNorm)
            for (usize P=0;P<Bytes.size();P+=4)
            {
                const uint32 V=uint32(Bytes[P])|(uint32(Bytes[P+1])<<8)|(uint32(Bytes[P+2])<<16)|(uint32(Bytes[P+3])<<24);
                Sample((V&1023)/1023.0); Sample(((V>>10)&1023)/1023.0); Sample(((V>>20)&1023)/1023.0); Sample((V>>30)/3.0);
            }
        else if (I.Format==ERHIFormat::R8G8B8A8_UNorm || I.Format==ERHIFormat::B8G8R8A8_UNorm || I.Format==ERHIFormat::R8G8B8A8_sRGB)
            for (auto V : Bytes) Sample(V/255.0);
        else return false;
    }
    std::ostringstream JSON;
    JSON.imbue(std::locale::classic()); JSON << std::setprecision(17);
    JSON << "{\"schemaVersion\":1,\"authority\":\"non-authoritative-interactive-preview\",\"hdrAppearanceMatch\":false,"
        << "\"softwareRevision\":" << Quote(SoftwareRevision) << ",\"requestId\":" << Completion.Request.RequestId
        << ",\"frameToken\":" << Completion.FrameToken << ",\"settingsGeneration\":" << I.SettingsGeneration
        << ",\"displayGeneration\":" << I.DisplayGeneration << ",\"outputGeneration\":" << I.OutputGeneration
        << ",\"width\":" << I.Width << ",\"height\":" << I.Height << ",\"format\":" << static_cast<unsigned>(I.Format)
        << ",\"outputProfile\":" << Quote(I.OutputProfile) << ",\"stage\":" << Quote(I.Stage)
        << ",\"includeUI\":" << (I.bIncludeUI ? "true" : "false") << ",\"payloadKind\":\"" << (Result.bPNG ? "SDR-PNG" : "numeric-raw")
        << "\",\"sourceByteCount\":" << Bytes.size() << ",\"payloadByteCount\":" << Result.Payload.size()
        << ",\"payloadSha256\":" << Quote(Asset::FAssetDigest::FromBytes(Result.Payload).ToLowerHex())
        << ",\"payloadStorage\":\"" << (Result.bPNG ? "beside-report" : "Build/InteractiveLab/Raw") << "\"";
    if (!Result.bPNG)
    {
        JSON << ",\"finiteComponents\":" << Finite << ",\"nonFiniteComponents\":" << NonFinite << ",\"minimum\":";
        if (Finite) JSON << Minimum; else JSON << "null";
        JSON << ",\"maximum\":"; if (Finite) JSON << Maximum; else JSON << "null";
    }
    JSON << "}\n";
    const auto Text=JSON.str(); if (Text.size()>4096) return false;
    Result.Report.assign(Text.begin(),Text.end()); Out=std::move(Result); return true;
}
}
