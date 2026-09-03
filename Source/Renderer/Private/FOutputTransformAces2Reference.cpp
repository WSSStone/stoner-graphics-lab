// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.
//
// This repository-owned double-precision transliteration follows the ACES 2
// forward algorithm frozen by Feature 029. The implementation structure was
// cross-checked against OpenColorIO v2.4.2 (6918fad), whose released ACES 2
// implementation is derived from the same Academy source authority. It is not
// linked to OpenColorIO and has no runtime dependency on it.

#include "FOutputTransformAces2Reference.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace Stoner::Renderer::Private
{
namespace
{

using f2 = std::array<double, 2>;
using f3 = std::array<double, 3>;
using m33f = std::array<double, 9>;

struct Chromaticities
{
    double X;
    double Y;
};

struct Primaries
{
    Chromaticities Red;
    Chromaticities Green;
    Chromaticities Blue;
    Chromaticities White;
};

inline double Lerp(double Lower, double Upper, double Weight)
{
    return Lower + (Upper - Lower) * Weight;
}

inline f3 f3_from_f(double Value)
{
    return {Value, Value, Value};
}

inline f3 mult_f_f3(double Value, const f3& Vector)
{
    return {Value * Vector[0], Value * Vector[1], Value * Vector[2]};
}

inline f3 mult_f3_f33(const f3& Vector, const m33f& Matrix)
{
    return {
        Vector[0] * Matrix[0] + Vector[1] * Matrix[1] + Vector[2] * Matrix[2],
        Vector[0] * Matrix[3] + Vector[1] * Matrix[4] + Vector[2] * Matrix[5],
        Vector[0] * Matrix[6] + Vector[1] * Matrix[7] + Vector[2] * Matrix[8]};
}

inline m33f mult_f33_f33(const m33f& Left, const m33f& Right)
{
    return {
        Left[0] * Right[0] + Left[1] * Right[3] + Left[2] * Right[6],
        Left[0] * Right[1] + Left[1] * Right[4] + Left[2] * Right[7],
        Left[0] * Right[2] + Left[1] * Right[5] + Left[2] * Right[8],
        Left[3] * Right[0] + Left[4] * Right[3] + Left[5] * Right[6],
        Left[3] * Right[1] + Left[4] * Right[4] + Left[5] * Right[7],
        Left[3] * Right[2] + Left[4] * Right[5] + Left[5] * Right[8],
        Left[6] * Right[0] + Left[7] * Right[3] + Left[8] * Right[6],
        Left[6] * Right[1] + Left[7] * Right[4] + Left[8] * Right[7],
        Left[6] * Right[2] + Left[7] * Right[5] + Left[8] * Right[8]};
}

inline m33f scale_f33(const m33f& Matrix, const f3& Scale)
{
    return {
        Matrix[0] * Scale[0], Matrix[1], Matrix[2],
        Matrix[3], Matrix[4] * Scale[1], Matrix[5],
        Matrix[6], Matrix[7], Matrix[8] * Scale[2]};
}

inline m33f invert_f33(const m33f& Matrix)
{
    const double A = Matrix[0];
    const double B = Matrix[1];
    const double C = Matrix[2];
    const double D = Matrix[3];
    const double E = Matrix[4];
    const double F = Matrix[5];
    const double G = Matrix[6];
    const double H = Matrix[7];
    const double I = Matrix[8];
    const double Determinant =
        A * (E * I - F * H) - B * (D * I - F * G) + C * (D * H - E * G);
    const double InverseDeterminant = 1.0 / Determinant;
    return {
        (E * I - F * H) * InverseDeterminant,
        (C * H - B * I) * InverseDeterminant,
        (B * F - C * E) * InverseDeterminant,
        (F * G - D * I) * InverseDeterminant,
        (A * I - C * G) * InverseDeterminant,
        (C * D - A * F) * InverseDeterminant,
        (D * H - E * G) * InverseDeterminant,
        (B * G - A * H) * InverseDeterminant,
        (A * E - B * D) * InverseDeterminant};
}

inline f3 WhiteXyz(const Chromaticities& White)
{
    return {White.X / White.Y, 1.0, (1.0 - White.X - White.Y) / White.Y};
}

inline m33f RGBtoXYZ_f33(const Primaries& Value)
{
    const m33f Unscaled = {
        Value.Red.X / Value.Red.Y,
        Value.Green.X / Value.Green.Y,
        Value.Blue.X / Value.Blue.Y,
        1.0, 1.0, 1.0,
        (1.0 - Value.Red.X - Value.Red.Y) / Value.Red.Y,
        (1.0 - Value.Green.X - Value.Green.Y) / Value.Green.Y,
        (1.0 - Value.Blue.X - Value.Blue.Y) / Value.Blue.Y};
    const f3 Scale = mult_f3_f33(WhiteXyz(Value.White), invert_f33(Unscaled));
    return {
        Unscaled[0] * Scale[0], Unscaled[1] * Scale[1], Unscaled[2] * Scale[2],
        Unscaled[3] * Scale[0], Unscaled[4] * Scale[1], Unscaled[5] * Scale[2],
        Unscaled[6] * Scale[0], Unscaled[7] * Scale[1], Unscaled[8] * Scale[2]};
}

inline m33f XYZtoRGB_f33(const Primaries& Value)
{
    return invert_f33(RGBtoXYZ_f33(Value));
}

inline m33f RGBtoRGB_f33(const Primaries& Source, const Primaries& Destination)
{
    return mult_f33_f33(XYZtoRGB_f33(Destination), RGBtoXYZ_f33(Source));
}

constexpr m33f Identity_M33 = {
    1.0, 0.0, 0.0,
    0.0, 1.0, 0.0,
    0.0, 0.0, 1.0};

struct TableBase
{
    static constexpr unsigned int base_index = 1;
    static constexpr unsigned int nominal_size = 360;
    static constexpr unsigned int total_size = nominal_size + 2;
    static constexpr unsigned int lower_wrap_index = 0;
    static constexpr unsigned int upper_wrap_index = base_index + nominal_size;
    static constexpr unsigned int first_nominal_index = base_index;
    static constexpr unsigned int last_nominal_index = upper_wrap_index - 1;

    double base_hue_for_position(unsigned int Position) const
    {
        return static_cast<double>(Position);
    }

    unsigned int hue_position_in_uniform_table(double WrappedHue) const
    {
        return static_cast<unsigned int>(WrappedHue);
    }

    unsigned int nominal_hue_position_in_uniform_table(double WrappedHue) const
    {
        return first_nominal_index + hue_position_in_uniform_table(WrappedHue);
    }
};

struct Table3D : TableBase, std::array<f3, TableBase::total_size>
{
};

struct Table1D : TableBase, std::array<double, TableBase::total_size>
{
};

struct JMhParams
{
    m33f MATRIX_RGB_to_CAM16_c;
    m33f MATRIX_CAM16_c_to_RGB;
    m33f MATRIX_cone_response_to_Aab;
    m33f MATRIX_Aab_to_cone_response;
    double F_L_n;
    double cz;
    double inv_cz;
    double A_w_J;
    double inv_A_w_J;
};

struct ToneScaleParams
{
    double n;
    double n_r;
    double g;
    double t_1;
    double c_t;
    double s_2;
    double u_2;
    double m_2;
    double forward_limit;
    double inverse_limit;
    double log_peak;
};

struct SharedCompressionParameters
{
    double limit_J_max;
    double model_gamma_inv;
    Table1D reach_m_table;
};

struct ResolvedSharedCompressionParameters
{
    double limit_J_max;
    double model_gamma_inv;
    double reachMaxM;
};

struct ChromaCompressParams
{
    double sat;
    double sat_thr;
    double compr;
    double chroma_compress_scale;
    [[maybe_unused]] static constexpr double cusp_mid_blend = 1.3;
};

struct HueDependantGamutParams
{
    double gamma_bottom_inv;
    f2 JMcusp;
    double gamma_top_inv;
    double focusJ;
    double analytical_threshold;
};

struct GamutCompressParams
{
    double mid_J;
    double focus_dist;
    double lower_hull_gamma_inv;
    std::array<int, 2> hue_linearity_search_range;
    Table1D hue_table;
    Table3D gamut_cusp_table;
};

constexpr double PI = 3.14159265358979;
constexpr double hue_limit = 360.0;
inline double _wrap_to_hue_limit(double Value)
{
    return Value < 0.0 ? Value + hue_limit : Value;
}
[[maybe_unused]] inline double wrap_to_hue_limit(double Hue)
{
    return _wrap_to_hue_limit(std::fmod(Hue, hue_limit));
}
inline constexpr double to_radians(double Value)
{
    return PI * Value / 180.0;
}
inline double _from_radians(double Value)
{
    return _wrap_to_hue_limit(180.0 * Value / PI);
}

constexpr double reference_luminance = 100.0;
constexpr double L_A = 100.0;
constexpr double Y_b = 20.0;
constexpr f3 surround = {0.9, 0.59, 0.9};
constexpr double J_scale = 100.0;
constexpr double cam_nl_Y_reference = 100.0;
constexpr double cam_nl_offset = 0.2713 * cam_nl_Y_reference;
constexpr double cam_nl_scale = 4.0 * cam_nl_Y_reference;
constexpr double chroma_compress = 2.4;
constexpr double chroma_compress_fact = 3.3;
constexpr double chroma_expand = 1.3;
constexpr double chroma_expand_fact = 0.69;
constexpr double chroma_expand_thr = 0.5;
constexpr double smooth_cusps = 0.12;
constexpr double smooth_m = 0.27;
constexpr double cusp_mid_blend = 1.3;
constexpr double focus_gain_blend = 0.3;
[[maybe_unused]] constexpr double focus_adjust_gain_inv = 1.0 / 0.55;
constexpr double focus_distance = 1.35;
constexpr double focus_distance_scaling = 1.75;
constexpr double compression_threshold = 0.75;
constexpr double gammaMinimum = 0.0;
constexpr double gammaMaximum = 5.0;
constexpr double gammaSearchStep = 0.4;
constexpr double gammaAccuracy = 1e-5;
constexpr int cuspCornerCount = 6;
constexpr int totalCornerCount = cuspCornerCount + 2;
constexpr int max_sorted_corners = 2 * cuspCornerCount;
constexpr double reach_cusp_tolerance = 1e-3;
constexpr double display_cusp_tolerance = 1e-7;

namespace CAM16
{
constexpr Primaries primaries = {
    {0.8336, 0.1735}, {2.3854, -1.4659}, {0.087, -0.125}, {0.333, 0.333}};
}

constexpr Primaries AcesAp0 = {
    {0.7347, 0.2653}, {0.0, 1.0}, {0.0001, -0.0770}, {0.32168, 0.33767}};
constexpr Primaries AcesAp1 = {
    {0.713, 0.293}, {0.165, 0.830}, {0.128, 0.044}, {0.32168, 0.33767}};
constexpr Primaries Rec709 = {
    {0.640, 0.330}, {0.300, 0.600}, {0.150, 0.060}, {0.3127, 0.3290}};
constexpr Primaries Rec2020 = {
    {0.708, 0.292}, {0.170, 0.797}, {0.131, 0.046}, {0.3127, 0.3290}};

//
// Table lookups
//
inline f3 lerp(const f3& lower, const f3& upper, const double t)
{
    return {
        Lerp(lower[0], upper[0], t),
        Lerp(lower[1], upper[1], t),
        Lerp(lower[2], upper[2], t)
    };
}

[[maybe_unused]] inline f3 lerp(const double lower[3], const double upper[3], const double t)
{
    return {
        Lerp(lower[0], upper[0], t),
        Lerp(lower[1], upper[1], t),
        Lerp(lower[2], upper[2], t)
    };
}

inline unsigned int midpoint(unsigned int a, unsigned int b)
{
    return (a + b) / 2;
}

inline double midpoint(double a, double b)
{
    return (a + b) / 2.f;
}

unsigned int lookup_hue_interval(double h, const Table1D &hues, const std::array<int, 2> & hue_linearity_search_range)
{
    // Search the given Table for the interval containing the desired hue
    // Returns the upper index of the interval

    // We can narrow the search range based on the hues being almost uniform
    unsigned int i = hues.nominal_hue_position_in_uniform_table(h);
    unsigned int i_lo = std::max(int(hues.lower_wrap_index), int(i) + hue_linearity_search_range[0]); // Should be nominal not lower_wrap?
    unsigned int i_hi = std::min(int(hues.upper_wrap_index), int(i) + hue_linearity_search_range[1]);

    while (i_lo + 1 < i_hi)
    {
        if (h > hues[i])
        {
            i_lo = i;
        }
        else
        {
            i_hi = i;
        }
        i = midpoint(i_lo, i_hi);
    }

    i_hi = std::max(1U, i_hi); // TODO: should not be needed if we initialise with the correct lo and hi

    return i_hi;
}

inline double interpolation_weight(double h, double h_lo, double h_hi)
{
    return (h - h_lo) / (h_hi - h_lo);
}

[[maybe_unused]] inline double interpolation_weight(double h, unsigned int h_lo, unsigned int h_hi)
{
    return (h - h_lo) / (h_hi - h_lo);
}

inline f3 cusp_from_table(unsigned int i_hi, double t, const Table3D &gt)
{
    return lerp(gt[i_hi-1], gt[i_hi], t);
}

double reach_m_from_table(double h, const Table1D &rt)
{
    const unsigned int base = rt.hue_position_in_uniform_table(h);
    const double t = h - base; // NOTE assumes uniform 1 degree 360 spacing
    const unsigned int i_lo = base + rt.first_nominal_index;
    const unsigned int i_hi = i_lo + 1; // NOTE assumes uniform 1 degree 360 spacing

    return Lerp(rt[i_lo], rt[i_hi], t);
}

//
// CAM
//

inline double _post_adaptation_cone_response_compression_fwd(double Rc)
{
    const double F_L_Y = std::pow(Rc, 0.42f);
    const double Ra    = (F_L_Y) / (cam_nl_offset + F_L_Y);
    return Ra;
}

inline double _post_adaptation_cone_response_compression_inv(double Ra)
{
    const double Ra_lim = std::min(Ra, 0.99);
    const double F_L_Y = (cam_nl_offset * Ra_lim) / (1.0f - Ra_lim);
    const double Rc    = std::pow(F_L_Y, 1.f / 0.42f);
    return Rc;
}


double post_adaptation_cone_response_compression_fwd(double v)
{
    const double abs_v = std::abs(v);
    const double Ra =  _post_adaptation_cone_response_compression_fwd(abs_v);
    // Note that std::copysign(1.f, 0.f) returns 1 but the CTL copysign(1.,0.) returns 0.
    // TODO: Should we change the behaviour?
    return std::copysign(Ra, v);
}

double post_adaptation_cone_response_compression_inv(double v)
{
    const double abs_v = std::abs(v);
    const double Rc    = _post_adaptation_cone_response_compression_inv(abs_v);
    return std::copysign(Rc, v);
}

inline double Achromatic_n_to_J(double A, const double cz)
{
    return J_scale * std::pow(A, cz);
}

inline double J_to_Achromatic_n(double J, const double inv_cz)
{
    return std::pow(J * (1.0f / J_scale), inv_cz);
}

// Optimization for achromatic values

inline double _A_to_Y(double A, const JMhParams &p)
{
    const double Ra = p.A_w_J * A;
    const double Y  = _post_adaptation_cone_response_compression_inv(Ra) / p.F_L_n;
    return Y;
}

inline double _J_to_Y(double abs_J, const JMhParams &p)
{
    return _A_to_Y(J_to_Achromatic_n(abs_J, p.inv_cz), p);
}

inline double _Y_to_J(double abs_Y, const JMhParams &p)
{
    const double Ra = _post_adaptation_cone_response_compression_fwd(abs_Y * p.F_L_n);
    const double J  = Achromatic_n_to_J(Ra * p.inv_A_w_J, p.cz);
    return J;
}

double Y_to_J(double Y, const JMhParams &p)
{
    const double abs_Y = std::abs(Y);
    const double J     = _Y_to_J(abs_Y, p);
    return std::copysign(J, Y);
}

f3 RGB_to_Aab(const f3 &RGB, const JMhParams &p)
{
    const f3 rgb_m = mult_f3_f33(RGB, p.MATRIX_RGB_to_CAM16_c);

    const f3 rgb_a = {
        post_adaptation_cone_response_compression_fwd(rgb_m[0]),
        post_adaptation_cone_response_compression_fwd(rgb_m[1]),
        post_adaptation_cone_response_compression_fwd(rgb_m[2])
    };

    const f3 Aab = mult_f3_f33(rgb_a, p.MATRIX_cone_response_to_Aab);
    return Aab;
}

f3 Aab_to_JMh(const f3 &Aab, const JMhParams &p)
{
    if (Aab[0] <= 0.f)
    {
        return {0.f, 0.f, 0.f};
    }
    const double J = Achromatic_n_to_J(Aab[0], p.cz);
    const double M = sqrt(Aab[1] * Aab[1] + Aab[2] * Aab[2]);
    const double h_rad = std::atan2(Aab[2], Aab[1]);
    double h = _from_radians(h_rad); // Call to unwrapped hue version due to atan2 limits

    return {J, M, h};
}

f3 RGB_to_JMh(const f3 &RGB, const JMhParams &p)
{
    const f3 Aab = RGB_to_Aab(RGB, p);
    const f3 JMh = Aab_to_JMh(Aab, p);
    return JMh;
}

f3 JMh_to_Aab(const f3 &JMh, const double &cos_hr, const double &sin_hr, const JMhParams &p)
{
    const double J = JMh[0];
    const double M = JMh[1];

    const double A = J_to_Achromatic_n(J, p.inv_cz);
    const double a = M * cos_hr;
    const double b = M * sin_hr;
    return {A, a, b};
}

f3 JMh_to_Aab(const f3 &JMh, const JMhParams &p)
{
    const double h = JMh[2];
    const double h_rad = to_radians(h);
    const double cos_hr = cos(h_rad);
    const double sin_hr = sin(h_rad);

    return JMh_to_Aab(JMh, cos_hr, sin_hr, p);
}

f3 Aab_to_RGB(const f3 &Aab, const JMhParams &p)
{
    const f3 rgb_a = mult_f3_f33(Aab, p.MATRIX_Aab_to_cone_response);

    const f3 rgb_m = {
        post_adaptation_cone_response_compression_inv(rgb_a[0]),
        post_adaptation_cone_response_compression_inv(rgb_a[1]),
        post_adaptation_cone_response_compression_inv(rgb_a[2])
    };

    const f3 rgb = mult_f3_f33(rgb_m, p.MATRIX_CAM16_c_to_RGB);
    return rgb;
}

f3 JMh_to_RGB(const f3 &JMh, const JMhParams &p)
{
    const f3 Aab = JMh_to_Aab(JMh, p);
    const f3 rgb = Aab_to_RGB(Aab, p);
    return rgb;
}

//
// Tonescale / Chroma compress
//

/*
//TODO: move to header
// https://stackoverflow.com/questions/6996764/fastest-way-to-do-horizontal-sse-vector-sum-or-other-reduction/35270026#35270026
// https://gcc.godbolt.org/#compilers:!((compiler:g6,options:%27-xc+-O3+-Wall+-fverbose-asm+-march%3Dhaswell+-mno-avx%27,sourcez:MQSwdgxgNgrgJgUwAQB4QFt3gC4CdwB0AFgHwBQoksiqAztnDseWQPStLZEi1I9JQQAa2QBZAIa5x2AOS0ANEgBGMbJwDuCcSLicA9kgh70ABxBRk2A0oTZsCXIb2ICbDtHFgA5khhgAZnq42H7SCFAAnkiIECCIvFa%2BtMjoegBu4ia0rLREMP4muuD0Wrp6/kipaURQWWT%2BUHrSSES0MOgA%2BlkdtMkAjAAUHR3ofQBMABxIaQCUSADeSEvLK6tr62vs00gAvEgA2kgAIkgAwkgAPkgAQkgAgkgAumQrw6OTSLn5y3sjnV/%2BBoILq0AZpRTgpAdUSiDoAZQAEgBVABiKIAMgBRAZjRQAZkUAAZFH0ZjMANxLLaHc4nK63B7PV4jcZTNroXhLX6YDriOBwEFgxQAimbDjs3h7Q5HADU51OMrpNxlDzuMtuTOWAI2XKhPKqCBqgoBwvatFFqy2B2W5yWStl8sVTypHAsai4yCMpnMDiQ4jSejifsq6WUUVwCBgtHAPgBLy1Zp1uz1nT5At6Awlwry/gp8aWEZCuDArx5EDS2F6HX8eLGmbNeYAvmR6o1mq12iCesk8UMWR9Zgt81D%2B2yc8m/iN0oa4DATILZpT1lslLgmnAIOJ6Ehwgh0AgwJWkAS%2BvokLjCcO3qzPonuan%2BQvs/k8yttet71O0oaoMac6aOVFLZuC8IgWnEKAKgAWhIAQ9HUcDIOHCUNk/NNu3rDln1zclh0LGBiw2Sdy0rWhq1rTDzVw5sWy2Wh0AgixHCMGhowALwQRQVDUKMYEYqJaEaTRHD8CxeicRAoPY5B%2BHEJATDXJQLHQVsmjUDtOm6Ig0z7d4pkHeYr1HW8OQnHltMfLIhWmV8E1Mj8Uw6CyBSsrMJVsgtbAIkt1mIisqxrOt3OoltQH8RAKmGO4ADUAA1hlU9t2TGABWAA2Lt/QAD101K0psodmT06YhN1P48o6Td6CyCqslZMEPJHYrqhAUCzPQCqECyvBxAgbB/FZJ8kFJSlgNasDWSM5qb3a3lLNBNIhIhECiAtZYtjTTgiGQUrJpWfDCI0rtegQXs0lZNakw2sAijAQQwEsbbPh7aYHGjPQwEUdRuAgMD%2BD0ExsAwCCkECRwYti4ctgGMADHULd7Fu%2BhcBgPqQA%2BhQ/Ruv0oCgJByi2vc/QjQmkAAFluLAwAwdoZjIZtgAPRh/BbNx2HZrY4D0GAlIQMguZ5iwWnZLp02SOtr0mXQ0jgOZFiTBWtjSZNDluK5GSmm8/AisqeW1hB/HABAXNBS6FY2TmPpkNR1CCXAIi%2B5APsiJJkDTOEjkUB7v0cKxUe4bxSa8SQlHELxkCUEAj3UKOwM8JAUQABT9flNY%2BAFsFMT8DSNKz9f8RRiIRwoF1li0tg9En%2BFhkNqlqOA04mXR30/KrsG6QpMxzTOTEag6fL8ysBX8NKyaGHl0NoOAwTgbCyRC2iOY5vhD1rMgcGFzsEDMWtuwQCXRxAJAsrligQHCg2RwhhKitZI/uFH3XOj8ExeqEbgOm3kBR4GLLFBPpcOoth4iggDBwnhdCwzAFBeIeBUZA2/PcOKAhbC8CjJ8f0bswz2CQDHLg3M1APWNjGYMVQKDhGSI3e%2B38yZPx6DmIEn8d51j/lCGE8JkRoixAMPoRJ8SKDGPPchN1z5UJMo/NCj4v4UQfmTf%2BjVJYTGobWOhAIgSNCYSAPoaVMKj0LuwxEqIMTYl4UgYkx4BHzxWFsOE8MTCk1KtgW2O5lIHkrGI9kKjJECmkUFdoeiWggFrH3LyhFB7RkGtGCinjBGAOsRwOEcJMRjFrg3eJ/dHJdR6n1TRMioBEjNjYpJcidzfhLPgmAbUo5OG9GJM8VQkb2D5PjCoclBD2CkHjEwWTcC6FwLWeQWVMDyEvDRDeR1fHdh7D0ISPQGK4xYggXSd9j4zEMisLKs1nK5JYfI3CGytnoV8b/PZeFQklnCVoyYPQgksKbKzReKdMiI1BmudASAJBSFkLwCGLQ7AmAAFzsHoG/acuAGjwQIF6VgABHGACB6DozANkPotY%2BgAE4%2BhkzSqwIg8EoJWCku0KCHV0pQTxfgNiH1sD8VYKisYDKyZ9EJMAeljLmWJXUiLboDEvnLI%2BCfQqywjDIrUIokyABFPiug9hZX2cKjGYrjKNCOHxPGex2RSr5PKl0gTVUgz2AMJAUFFAmuPniJAMpj4AHZ/4pKtVlfKdMVgiu3OK7g%2Br1WORzr%2BVy7QtWz0ldKxqdF2ieuTEas1ZqsqngdRah1KVLU2v/oSJNWV7XH1oQ6p1w5XVKuKuycN3jBQqrVYoD1aqQ2ugMIa41pr63H1TQ6jNWUs3HxzS6xVTUbyNGTIWtVOqkBjQjXW0d0bY3mrTYmh11qkDOoVaK7tHxuCzTURYY0Yay0mU9USGNVaTIjqjQ2rKTbj4TvTWm%2BNmap1pvyjOuduau3ivZLNSeoJGjlpAOXDg6EsjRANlob5hgoCeC8HIT4DChYAyBlgDiuAzlFguWWfyZFAqYXuUAA%3D%3D)),filterAsm:(binary:!t,commentOnly:!t,directives:!t,intel:!t,labels:!t),version:3
// Alternate https://github.com/vectorclass/version2/blob/master/vectorf128.h#L1048

#if OCIO_USE_SSE2 || OCIO_USE_AVX
inline double hsum_ps_sse1(__m128 v) {                              // v    = [   D   C |   B   A ]
    __m128 shuf   = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1)); // shuf = [   C   D |   A   B ]
    __m128 sums   = _mm_add_ps(v, shuf);                           // sums = [ D+C C+D | B+A A+B ]
    shuf          = _mm_movehl_ps(shuf, sums);                     // shuf = [   C   D | D+C C+D ]
    sums          = _mm_add_ss(sums, shuf);                        // sums = [ D+C C+D | B+A A+B+C+D ]
    return    _mm_cvtss_f32(sums);                                 // A+B+C+D
}
#endif

#ifdef OCIO_USE_AVX
inline double hsum256_ps_avx(__m256 v) {         // v     = [   H   G |   F   E |   D   C |   B   A ]
    __m128 vlow  = _mm256_castps256_ps128(v);   // vlow  = [   D   C |   B   A ]
    __m128 vhigh = _mm256_extractf128_ps(v, 1); // vhigh = [   H   G |   F   E ]
    __m128 v128  = _mm_add_ps(vlow, vhigh);     // v128  = [ H+D G+C | F+B E+A ]
    return hsum_ps_sse1(v128);
}
#endif
#endif
*/

double chroma_compress_norm(double cos_hr1, double sin_hr1, double chroma_compress_scale)
{
    const double cos_hr2 = 2.0f * cos_hr1 * cos_hr1 - 1.0f;
    const double sin_hr2 = 2.0f * cos_hr1 * sin_hr1;
    const double cos_hr3 = 4.0f * cos_hr1 * cos_hr1 * cos_hr1 - 3.0f * cos_hr1;
    const double sin_hr3 = 3.0f * sin_hr1 - 4.0f * sin_hr1 * sin_hr1 * sin_hr1;

    constexpr unsigned int AVX_ALIGNMENT = 32;
    alignas(AVX_ALIGNMENT) const double trig_angles_hr[8] = {
        cos_hr1, cos_hr2, cos_hr3, 0.0f,
        sin_hr1, sin_hr2, sin_hr3, 1.0f
    };
    alignas(AVX_ALIGNMENT) static constexpr double weights[8] = { // TODO: investigate reordering of the entries so we are summing equal magnitude values first?
        11.34072f, 16.46899f, 7.88380f, 0.0f,
        14.66441f, -6.37224f, 9.19364f, 77.12896f
    };

    /*
    // TODO: benchmark this across multiple platforms to justify the multiple code paths.
#if OCIO_USE_SSE2
#if OCIO_USE_AVX
    __m256 trigs        = _mm256_load_ps(trig_angles_hr);
    __m256 trig_weights = _mm256_load_ps(weights);
    __m256 t1           = _mm256_mul_ps(trigs, trig_weights);
    const double M       = hsum256_ps_avx(t1);
#else
    __m128 cosines        = _mm_load_ps(trig_angles_hr);
    __m128 sines          = _mm_load_ps(&trig_angles_hr[4]);
    __m128 cosine_weights = _mm_load_ps(weights);
    __m128 sine_weights   = _mm_load_ps(&weights[4]);

    __m128 t1     = _mm_mul_ps(cosines, cosine_weights);
    __m128 t2     = _mm_mul_ps(sines, sine_weights);
    __m128 t3     = _mm_add_ps(t1, t2); // TODO use fmadd_ps_sse2 from Lut1DOpCPU_SSE2.cpp ?
    const double M = hsum_ps_sse1(t3);
#endif
#else
    */
    const double M = weights[0] * trig_angles_hr[0] +
                    weights[1] * trig_angles_hr[1] +
                    weights[2] * trig_angles_hr[2] +
                    weights[4] * trig_angles_hr[4] +
                    weights[5] * trig_angles_hr[5] +
                    weights[6] * trig_angles_hr[6] +
                    weights[7];
    /*
#endif
    */

    return M * chroma_compress_scale; // TODO: is it worth prescaling the above weights?
}

inline double toe_fwd( double x, double limit, double k1_in, double k2_in)
{
    if (x > limit)
    {
        return x;
    }

    const double k2 = std::max(k2_in, 0.001);
    const double k1 = sqrt(k1_in * k1_in + k2 * k2);
    const double k3 = (limit + k1) / (limit + k2);

    const double minus_b = k3 * x - k1;
    const double minus_ac = k2 * k3 * x; // a is 1.0
    return 0.5f * (minus_b + sqrt(minus_b * minus_b + 4.f * minus_ac)); // a is 1.0, mins_b squared == b^2
}

inline double toe_inv( double x, double limit, double k1_in, double k2_in)
{
    if (x > limit)
    {
        return x;
    }

    const double k2 = std::max(k2_in, 0.001);
    const double k1 = sqrt(k1_in * k1_in + k2 * k2);
    const double k3 = (limit + k1) / (limit + k2);
    return (x * x + k1 * x) / (k3 * (x + k2));
}

template <bool inverse>
inline double aces_tonescale(const double Y_in, const ToneScaleParams &pt)
{
    if (inverse)
    {
        const double Y_ts_norm = Y_in / reference_luminance; // TODO
        const double Z = std::max(0.0, std::min(pt.inverse_limit, Y_ts_norm)); // TODO: could eliminate max in the context of the full tonescale
        const double f = (Z + sqrt(Z * (4.f * pt.t_1 + Z))) / 2.f;
        const double Y = pt.s_2 / (std::pow((pt.m_2 / f), (1.f / pt.g)) - 1.f); // TODO: investigate precomputing reciprocal m_2  and a negative power? may swap a division for a multiply? std::pow(pt.m_2_recip * f, (-1.f / pt.g))
        return Y;
    }

    const double f    = pt.m_2 * std::pow(Y_in / (Y_in + pt.s_2), pt.g);
    const double Y_ts = std::max(0.0, f * f / (f + pt.t_1)) * pt.n_r;  // max prevents -ve values being output also handles division by zero possibility // TODO: can we eliminate the n_r scaling? should it be reference_luminance
    return Y_ts;
}

template <bool inverse>
inline double tonescale(const double J, const JMhParams &p, const ToneScaleParams &pt) // TODO: consider computing tonescale from and to A rather than J to avoid extra pow() calls
{
    // Tonescale applied in Y (convert to and from J)
    const double J_abs = std::abs(J);
    const double Y_in  = _J_to_Y(J_abs, p);
    const double Y_out = aces_tonescale<inverse>(Y_in, pt);
    const double J_out = _Y_to_J(Y_out, p);
    return std::copysign(J_out, J);
}

[[maybe_unused]] double tonescale_fwd(const double J, const JMhParams &p, const ToneScaleParams &pt)
{
    return tonescale<false>(J, p, pt);
}

[[maybe_unused]] double tonescale_inv(const double J, const JMhParams &p, const ToneScaleParams &pt)
{
    return tonescale<true>(J, p, pt);
}


template <bool inverse>
inline double tonescale_A_to_J(const double A, const JMhParams &p, const ToneScaleParams &pt) // TODO: consider computing tonescale from and to A rather than J to avoid extra pow() calls
{
    const double Y_in  = _A_to_Y(A, p);
    const double Y_out = aces_tonescale<inverse>(Y_in, pt);
    const double J_out = _Y_to_J(Y_out, p);
    return std::copysign(J_out, A);
}

double tonescale_A_to_J_fwd(const double A, const JMhParams &p, const ToneScaleParams &pt)
{
    return tonescale_A_to_J<false>(A, p, pt);
}

f3 chroma_compress_fwd(const f3 &JMh, const double J_ts, const double Mnorm, const ResolvedSharedCompressionParameters &pr, const ChromaCompressParams &pc)
{
    const double J = JMh[0];
    const double M = JMh[1];
    const double h = JMh[2];

    double M_cp = M;

    if (M != 0.0)
    {
        const double nJ = J_ts / pr.limit_J_max;
        const double snJ = std::max(0.0, 1.0 - nJ);
        const double limit = std::pow(nJ, pr.model_gamma_inv) * pr.reachMaxM / Mnorm;

        M_cp = M * std::pow(J_ts / J, pr.model_gamma_inv);
        M_cp = M_cp / Mnorm;
        M_cp = limit - toe_fwd(limit - M_cp, limit - 0.001f, snJ * pc.sat, sqrt(nJ * nJ + pc.sat_thr));
        M_cp = toe_fwd(M_cp, limit, nJ * pc.compr, snJ);
        M_cp = M_cp * Mnorm;
    }

    return {J_ts, M_cp, h};
}

[[maybe_unused]] f3 chroma_compress_inv(const f3 &JMh, const double J, const double Mnorm, const ResolvedSharedCompressionParameters &pr, const ChromaCompressParams &pc)
{
    const double J_ts = JMh[0];
    const double M_cp = JMh[1];
    const double h    = JMh[2];
    double M = M_cp;

    if (M_cp != 0.0)
    {
        const double nJ = J_ts / pr.limit_J_max;
        const double snJ = std::max(0.0, 1.0 - nJ);
        const double limit = std::pow(nJ, pr.model_gamma_inv) * pr.reachMaxM / Mnorm;

        M = M_cp / Mnorm;
        M = toe_inv(M, limit, nJ * pc.compr, snJ);
        M = limit - toe_inv(limit - M, limit - 0.001f, snJ * pc.sat, sqrt(nJ * nJ + pc.sat_thr));
        M = M * Mnorm;
        M = M * std::pow(J_ts / J, -pr.model_gamma_inv);
    }

    return {J, M, h};
}

inline double model_gamma(void)
{
    // c * z nonlinearity
    return surround[1] * (1.48f + sqrt(Y_b / reference_luminance));
}

JMhParams init_JMhParams(const Primaries &prims)
{
    const m33f base_cone_response_to_Aab = {
        2.0f, 1.0f, 1.0f / 20.0f,
        1.0f, -12.0f / 11.0f, 1.0f / 11.0f,
        1.0f / 9.0f, 1.0f / 9.0f, -2.0f / 9.0f
    };

    const m33f MATRIX_16 = XYZtoRGB_f33(CAM16::primaries);
    const m33f RGB_to_XYZ = RGBtoXYZ_f33(prims);
    const f3 XYZ_w = mult_f3_f33(f3_from_f(reference_luminance), RGB_to_XYZ);

    const double Y_W = XYZ_w[1];

    const f3 RGB_w = mult_f3_f33(XYZ_w, MATRIX_16);

    // Viewing condition dependent parameters
    constexpr double K = 1.f / (5.f * L_A + 1.f);
    constexpr double K4 = K * K * K * K;
    const double F_L = 0.2f * K4 * (5.f * L_A) + 0.1f * std::pow((1.f - K4), 2.f) * std::pow(5.f * L_A, 1.f/3.f);

    const double F_L_n = F_L / reference_luminance;
    const double cz    = model_gamma();
    const double inv_cz = 1.0f / cz;

    const f3 D_RGB = {
        F_L_n * Y_W / RGB_w[0],
        F_L_n * Y_W / RGB_w[1],
        F_L_n * Y_W / RGB_w[2]
    };

    const f3 RGB_WC {
        D_RGB[0] * RGB_w[0],
        D_RGB[1] * RGB_w[1],
        D_RGB[2] * RGB_w[2]
    };

    const f3 RGB_AW = {
        post_adaptation_cone_response_compression_fwd(RGB_WC[0]),
        post_adaptation_cone_response_compression_fwd(RGB_WC[1]),
        post_adaptation_cone_response_compression_fwd(RGB_WC[2])
    };

    const m33f cone_response_to_Aab = mult_f33_f33(scale_f33(Identity_M33, f3_from_f(cam_nl_scale)), base_cone_response_to_Aab); // TODO: this scale maybe ill conditioning the matrix
    const double A_w   = cone_response_to_Aab[0] * RGB_AW[0] + cone_response_to_Aab[1] * RGB_AW[1] + cone_response_to_Aab[2] * RGB_AW[2];
    const double A_w_J = _post_adaptation_cone_response_compression_fwd(F_L);
    const double inv_A_w_J = 1.0f / A_w_J;

    // TODO: evaluate the condition number of the below matrices and consider extracting a power of 2 scale out of them may improve noise behaviour
    //       power of 2 to make cost of extra scaling limited vs generalised multiply/divide ?

    // Note we are prescaling the CAM16 LMS responses to directly provide for chromatic adaptation.
    const m33f MATRIX_RGB_to_CAM16 = mult_f33_f33(RGBtoRGB_f33(prims, CAM16::primaries), scale_f33(Identity_M33, f3_from_f(reference_luminance)));
    const m33f MATRIX_RGB_to_CAM16_c = mult_f33_f33(scale_f33(Identity_M33, D_RGB), MATRIX_RGB_to_CAM16);
    const m33f MATRIX_CAM16_c_to_RGB = invert_f33(MATRIX_RGB_to_CAM16_c);

    const m33f MATRIX_cone_response_to_Aab = {
        cone_response_to_Aab[0] / A_w,                cone_response_to_Aab[1] / A_w,                cone_response_to_Aab[2] / A_w,
        cone_response_to_Aab[3] * 43.f * surround[2], cone_response_to_Aab[4] * 43.f * surround[2], cone_response_to_Aab[5] * 43.f * surround[2],
        cone_response_to_Aab[6] * 43.f * surround[2], cone_response_to_Aab[7] * 43.f * surround[2], cone_response_to_Aab[8] * 43.f * surround[2],
    };
    const m33f MATRIX_Aab_to_cone_response = invert_f33(MATRIX_cone_response_to_Aab);

    JMhParams p = {
        MATRIX_RGB_to_CAM16_c,
        MATRIX_CAM16_c_to_RGB,
        MATRIX_cone_response_to_Aab,
        MATRIX_Aab_to_cone_response,
        F_L_n,
        cz,
        inv_cz,
        A_w_J,
        inv_A_w_J
    };
   return p;
}

inline f3 generate_unit_cube_cusp_corners(const unsigned int corner)
{
    // Generation order R, Y, G, C, B, M to ensure hues rotate in correct order
    return {double(static_cast<unsigned int>(((corner + 1) % cuspCornerCount) < 3)),
            double(static_cast<unsigned int>(((corner + 5) % cuspCornerCount) < 3)),
            double(static_cast<unsigned int>(((corner + 3) % cuspCornerCount) < 3))
            };
}

void build_limiting_cusp_corners_tables(std::array<f3, totalCornerCount>& RGB_corners, std::array<f3, totalCornerCount>& JMh_corners,
                               const JMhParams &params, const double peakLuminance)
{
    // We calculate the RGB and JMh values for the limiting gamut cusp corners
    // They are then arranged into a cycle with the lowest JMh value at [1] to allow for hue wrapping
    std::array<f3, cuspCornerCount> temp_RGB_corners;
    std::array<f3, cuspCornerCount> temp_JMh_corners;
    unsigned int min_index = 0;
    for (unsigned int i = 0; i != cuspCornerCount; ++i)
    {
      temp_RGB_corners[i] = mult_f_f3(peakLuminance / reference_luminance, generate_unit_cube_cusp_corners(i));
      temp_JMh_corners[i] = RGB_to_JMh(temp_RGB_corners[i], params);
      if (temp_JMh_corners[i][2] < temp_JMh_corners[min_index][2])
        min_index = i;
    }

    // Rotate entries placing lowest at [1] (not [0])
    for (unsigned int i = 0; i != cuspCornerCount; ++i)
    {
      RGB_corners[i + 1] = temp_RGB_corners[(i + min_index) % cuspCornerCount];
      JMh_corners[i + 1] = temp_JMh_corners[(i + min_index) % cuspCornerCount];
    }

    // Copy end elements to create a cycle
    RGB_corners[0]                   = RGB_corners[cuspCornerCount];
    RGB_corners[cuspCornerCount + 1] = RGB_corners[1];
    JMh_corners[0]                   = JMh_corners[cuspCornerCount];
    JMh_corners[cuspCornerCount + 1] = JMh_corners[1];

    // Wrap the hues, to maintain monotonicity these entries will fall outside [0.0, hue_limit)
    JMh_corners[0][2]                   = JMh_corners[0][2] - hue_limit;
    JMh_corners[cuspCornerCount + 1][2] = JMh_corners[cuspCornerCount + 1][2] + hue_limit;
}

void find_reach_corners_table(std::array<f3, totalCornerCount>& JMh_corners, const JMhParams &params, const double limitJ, const double maximum_source)
{
    // We need to find the value of JMh that corresponds to limitJ for each corner
    // This is done by scaling the unit corners converting to JMh until the J value is near the limitJ
    // As an optimisation we use the equivalent Achromatic value to search for the J value and avoid the
    // non-linear transform during the search
    // Strictly speaking we should only need to find the R, G and  B "corners" as the reach is unbounded and
    // as such does not form a cube, but is formed by the transformed 3 lower planes of the cube and
    // the plane at J = limitJ
    std::array<f3, cuspCornerCount> temp_JMh_corners;
    const double limitA = J_to_Achromatic_n(limitJ, params.inv_cz);

    unsigned int min_index = 0;
    for (unsigned int i = 0; i != cuspCornerCount; ++i)
    {
        const f3 rgb_vector = generate_unit_cube_cusp_corners(i);

        double lower = 0.0f;
        double upper = maximum_source;
        while ((upper - lower) > reach_cusp_tolerance)
        {
            double       test        = midpoint(lower, upper);
            const f3    test_corner = mult_f_f3(test, rgb_vector);
            const double A           = RGB_to_Aab(test_corner, params)[0];
            if (A < limitA)
            {
                lower = test;
            }
            else
            {
                upper = test;
            }
            if (A == limitA)
                break;
        }
        temp_JMh_corners[i] = RGB_to_JMh(mult_f_f3(upper, rgb_vector), params);

        if (temp_JMh_corners[i][2] < temp_JMh_corners[min_index][2])
            min_index = i;
    }

    // Rotate entries placing lowest at [1] (not [0]) // TODO: could use std::rotate_copy or even the ranges vs in C++20
    for (unsigned int i = 0; i != cuspCornerCount; ++i)
    {
      JMh_corners[i + 1] = temp_JMh_corners[(i + min_index) % cuspCornerCount];
    }

    // Copy end elements to create a cycle
    JMh_corners[0]                   = JMh_corners[cuspCornerCount];
    JMh_corners[cuspCornerCount + 1] = JMh_corners[1];

    // Wrap the hues, to maintain monotonicity these entries will fall outside [0.0, hue_limit)
    JMh_corners[0][2]                   = JMh_corners[0][2] - hue_limit;
    JMh_corners[cuspCornerCount + 1][2] = JMh_corners[cuspCornerCount + 1][2] + hue_limit;
}

unsigned int extract_sorted_cube_hues(std::array<double, max_sorted_corners>& sorted_hues,
                             const std::array<f3, totalCornerCount>& reach_JMh, const std::array<f3, totalCornerCount>& display_JMh)
{
    // Basic merge of 2 sorted arrays, extracting the unique hues.
    // Return the count of the unique hues
    //TODO: use STL for this and similar functions
    unsigned int idx         = 0;
    unsigned int reach_idx   = 1;
    unsigned int display_idx = 1;
    while ((reach_idx < (cuspCornerCount + 1)) || (display_idx < (cuspCornerCount + 1)))
    {
        const double reach_hue   = reach_JMh[reach_idx][2];
        const double display_hue = display_JMh[display_idx][2];
        if (reach_hue == display_hue)
        {
            sorted_hues[idx] = reach_hue;
            ++reach_idx;
            ++display_idx; // When equal consume both
        }
        else
        {
            if (reach_hue < display_hue)
            {
                sorted_hues[idx] = reach_hue;
                ++reach_idx;
            }
            else
            {
                sorted_hues[idx] = display_hue;
                ++display_idx;
            }
      }
      ++idx;
    }
    return idx;
}

void build_hue_sample_interval(const unsigned int samples, const double lower, const double upper, Table1D &hue_table, const unsigned int base)
{
    const double delta = (upper - lower) / double(samples);
    for (unsigned int i = 0; i != samples; ++i)
    {
        hue_table[base + i] = lower + double(i) * delta;
    }
}

void build_hue_table(Table1D &hue_table, const std::array<double, max_sorted_corners>& sorted_hues, const unsigned int unique_hues)
{
    const double ideal_spacing = hue_table.nominal_size / hue_limit;
    std::array<unsigned int, 2 * cuspCornerCount + 2> samples_count = {};
    unsigned int last_idx  = std::numeric_limits<unsigned int>::max();
    unsigned int min_index = sorted_hues[0] == 0.0f ? 0 : 1; // Ensure we can always sample at 0.0 hue
    for (unsigned int hue_idx = 0; hue_idx != unique_hues; ++hue_idx)
    {
        // BUG: "hue_table.size - 1" will fail if we have multiple hues mapping near the top of the table
        unsigned int nominal_idx = std::min(std::max(static_cast<unsigned int>(std::round(sorted_hues[hue_idx] * ideal_spacing)), min_index), hue_table.nominal_size - 1);
        if (last_idx == nominal_idx)
        {
            // Last two hues should sample at same index, need to adjust them
            // Adjust previous sample down if we can
            if (hue_idx > 1 && samples_count[hue_idx - 2] != (samples_count[hue_idx - 1] - 1))
            {
                samples_count[hue_idx - 1] = samples_count[hue_idx - 1] - 1;
            }
            else
            {
                nominal_idx = nominal_idx + 1;
            }
        }
        samples_count[hue_idx] = std::min(nominal_idx, hue_table.nominal_size - 1U);
        last_idx = min_index = nominal_idx;
    }

    unsigned int total_samples = 0;
    // Special cases for ends
    unsigned int i = 0;
    build_hue_sample_interval(samples_count[i], 0.0f, sorted_hues[i], hue_table, total_samples + 1);
    total_samples += samples_count[i];
    for (++i; i != unique_hues; ++i)
    {
        const unsigned int samples = samples_count[i] - samples_count[i - 1];
        build_hue_sample_interval(samples, sorted_hues[i - 1], sorted_hues[i], hue_table, total_samples + 1);
        total_samples += samples;
    }
    // BUG: could break if we are unlucky with samples all being used up by this point
    build_hue_sample_interval(hue_table.nominal_size - total_samples, sorted_hues[i - 1], hue_limit, hue_table, total_samples + 1);

    hue_table[hue_table.lower_wrap_index] = hue_table[hue_table.last_nominal_index] - hue_limit;
    hue_table[hue_table.upper_wrap_index] = hue_table[hue_table.first_nominal_index] + hue_limit;
}

std::array<double, 2> find_display_cusp_for_hue(double hue, const std::array<f3, totalCornerCount>& RGB_corners, const std::array<f3, totalCornerCount>& JMh_corners,
                                               const JMhParams &params, std::array<double, 2> & previous)
{
    // This works by finding the required line segment between two of the XYZ cusp corners, then binary searching
    // along the line calculating the JMh of points along the line till we find the required value.
    // All values on the line segments are valid cusp locations.

    unsigned int upper_corner = 1;
    for (unsigned int i = upper_corner; i != totalCornerCount; ++i) // TODO: binary search?
    {
        if (JMh_corners[i][2] > hue)
        {
            upper_corner = i;
            break;
        }
    }
    const unsigned int lower_corner = upper_corner - 1;

    // hue should now be within [lower_corner, upper_corner), handle exact match
    if (JMh_corners[lower_corner][2] == hue)
    {
        return {JMh_corners[lower_corner][0], JMh_corners[lower_corner][1]};
    }

    // search by lerping between RGB corners for the hue
    const f3 cusp_lower = RGB_corners[lower_corner];
    const f3 cusp_upper = RGB_corners[upper_corner];
    f3       sample;

    double sample_t;
    double lower_t = (upper_corner == previous[0]) ? previous[1] : 0.0f; // If we are still on the same segment start from where we left off
    double upper_t = 1.0f;

    f3 JMh;

    // There is an edge case where we need to search towards the range when across the [0.0f, hue_limit) boundary
    // each edge needs the directions swapped. This is handled by comparing against the appropriate corner to make
    // sure we are still in the expected range between the lower and upper corner hue limits
    while ((upper_t - lower_t) > display_cusp_tolerance)
    {
        sample_t = midpoint(lower_t, upper_t);
        sample   = lerp(cusp_lower, cusp_upper, sample_t);
        JMh      = RGB_to_JMh(sample, params);
        if (JMh[2] < JMh_corners[lower_corner][2])
        {
            upper_t = sample_t;
        }
        else if (JMh[2] >= JMh_corners[upper_corner][2])
        {
            lower_t = sample_t;
        }
        else if (JMh[2] > hue)
        {
            upper_t = sample_t;
        }
        else
        {
            lower_t = sample_t;
        }
    }

    // Use the midpoint of the final interval for the actual samples
    sample_t = midpoint(lower_t, upper_t);
    sample   = lerp(cusp_lower, cusp_upper, sample_t);
    JMh      = RGB_to_JMh(sample, params);

    previous[0] = double(upper_corner);
    previous[1] = sample_t;

    return {JMh[0], JMh[1]};
}

Table3D build_cusp_table(const Table1D& hue_table, const std::array<f3, totalCornerCount>& RGB_corners, const std::array<f3, totalCornerCount>& JMh_corners, const JMhParams &params)
{
    std::array<double, 2> previous = {0.0f, 0.0f};
    Table3D output_table;
    for (unsigned int i = output_table.first_nominal_index; i != output_table.upper_wrap_index; ++i)
    {
      const double hue = hue_table[i];
      const std::array<double, 2> JM = find_display_cusp_for_hue(hue, RGB_corners, JMh_corners, params, previous);
      output_table[i][0] = JM[0];
      output_table[i][1] = JM[1] * (1.f + smooth_m * smooth_cusps);;
      output_table[i][2] = hue;
    }

    // Copy extra entries to ease the code to handle hues wrapping around
    output_table[output_table.lower_wrap_index][0] = output_table[output_table.last_nominal_index][0];
    output_table[output_table.lower_wrap_index][1] = output_table[output_table.last_nominal_index][1];
    output_table[output_table.lower_wrap_index][2] = hue_table[hue_table.lower_wrap_index];
    output_table[output_table.upper_wrap_index][0] = output_table[output_table.first_nominal_index][0];
    output_table[output_table.upper_wrap_index][1] = output_table[output_table.first_nominal_index][1];
    output_table[output_table.upper_wrap_index][2] = hue_table[hue_table.upper_wrap_index];
    return output_table;
}

Table3D make_uniform_hue_gamut_table(const JMhParams &reach_params, const JMhParams &params, double peakLuminance, double forward_limit, const SharedCompressionParameters& sp, Table1D& hue_table)
{
    // The principal here is to sample the hues as uniformly as possible, whilst ensuring we sample the corners
    // of the limiting gamut and the reach primaries at limit J Max
    //
    // The corners are calculated then the hues are extracted and merged to form a unique sorted hue list
    // We then build the hue table from the list, those hues are then used to compute the JMh od the limiting
    // gamut cusp.

    std::array<f3, totalCornerCount> reach_JMh_corners;
    std::array<f3, totalCornerCount> limiting_RGB_corners;
    std::array<f3, totalCornerCount> limiting_JMh_corners;
    std::array<double, max_sorted_corners> sorted_hues;

    find_reach_corners_table(reach_JMh_corners, reach_params, sp.limit_J_max, forward_limit);
    build_limiting_cusp_corners_tables(limiting_RGB_corners, limiting_JMh_corners, params, peakLuminance);
    const unsigned int unique_hues = extract_sorted_cube_hues(sorted_hues, reach_JMh_corners, limiting_JMh_corners);
    build_hue_table(hue_table, sorted_hues, unique_hues);
    return build_cusp_table(hue_table, limiting_RGB_corners, limiting_JMh_corners, params);
}

bool any_below_zero(const f3 &rgb)
{
    return (rgb[0] < 0. || rgb[1] < 0. || rgb[2] < 0.);
}

Table1D make_reach_m_table(const JMhParams &params, const double limit_J_max)
{
    Table1D gamutReachTable{};

    for (unsigned int i = 0; i < gamutReachTable.nominal_size; i++) {
        const double hue = gamutReachTable.base_hue_for_position(i);

        constexpr double search_range = 50.f;
        constexpr double search_maximum = 1300.f; // TODO: magic limit
        double low = 0.f;
        double high = low + search_range;
        bool outside = false;

        while ((outside != true) & (high < search_maximum))
        {
            const f3 searchJMh = {limit_J_max, high, hue};
            const f3 newLimitRGB = JMh_to_RGB(searchJMh, params);
            outside = any_below_zero(newLimitRGB);
            if (outside == false)
            {
                low = high;
                high = high + search_range;
            }
        }

        while (high-low > 1e-2) // TODO:tolerance as constexpr
        {
            const double sampleM = (high + low) / 2.f;
            const f3 searchJMh = {limit_J_max, sampleM, hue};
            const f3 newLimitRGB = JMh_to_RGB(searchJMh, params);
            outside = any_below_zero(newLimitRGB);
            if (outside)
            {
                high = sampleM;
            }
            else
            {
                low = sampleM;
            }
        }

        gamutReachTable[i + gamutReachTable.base_index] = high;
    }
    gamutReachTable[gamutReachTable.lower_wrap_index] = gamutReachTable[gamutReachTable.last_nominal_index];
    gamutReachTable[gamutReachTable.upper_wrap_index] = gamutReachTable[gamutReachTable.first_nominal_index];

    return gamutReachTable;
}

inline bool outside_hull(const f3 &rgb, double maxRGBtestVal)
{
    // limit value, once we cross this value, we are outside of the top gamut shell
    return rgb[0] > maxRGBtestVal || rgb[1] > maxRGBtestVal || rgb[2] > maxRGBtestVal;
}

inline double get_focus_gain(double J, double analytical_threshold, double limit_J_max, double focus_dist)
{
    double gain = limit_J_max * focus_dist;
    if (J > analytical_threshold)
    {
        // Approximate inverse required above threshold due to the introduction of J in the calculation
        double gain_adjustment = log10((limit_J_max - analytical_threshold) / std::max(0.0001, limit_J_max - J));
        //gain = std::pow(gain, focus_adjust_gain_inv) + 1.f;
        gain_adjustment = gain_adjustment * gain_adjustment + 1.f;
        gain = gain * gain_adjustment;
    }
    return gain;
}

double solve_J_intersect(double J, double M, double focusJ, double maxJ, double slope_gain) // TODO: eval placing maxJ as last patrameter?
{
    const double M_scaled = M / slope_gain;
    const double a = M_scaled / focusJ;

    if (J < focusJ)
    {
        const double b = 1.f - M_scaled;
        const double c = -J;
        const double det = b * b - 4.f * a * c;
        const double root = sqrt(det);
        return -2.f * c / (b + root);
    }
    else
    {
        const double b = -(1.f + M_scaled + maxJ * a);
        const double c = maxJ * M_scaled + J;
        const double det = b * b - 4.f * a * c;
        const double root = sqrt(det);
        return -2.f * c / (b - root);
    }
}

// Smooth minimum about the scaled reference, based upon a cubic polynomial
inline double smin_scaled(double a, double b, double scale_reference)
{
    const double s_scaled = smooth_cusps * scale_reference;
    const double h = std::max(s_scaled - std::abs(a - b), 0.0) / s_scaled;
    return std::min(a, b) - h * h * h * s_scaled * (1.f / 6.f);
}

inline double compute_compression_vector_slope(const double intersectJ, const double focusJ, const double limitJmax, const double slope_gain)
{
    const double direction_scaler = (intersectJ < focusJ) ? intersectJ : (limitJmax - intersectJ); // TODO < vs <=
    return direction_scaler * (intersectJ - focusJ) / (focusJ * slope_gain);
}

inline double estimate_line_and_boundary_intersection_M(const double J_axis_intersect, const double slope, const double inv_gamma,
                                                       const double J_max, const double M_max, const double J_intersection_reference)
{
    // Line defined by     J = slope * x + J_axis_intersect
    // Boundary defined by J = J_max * (x / M_max) ^ (1/inv_gamma)
    // Approximate as we do not want to iteratively solve intersection of a straight line and an exponential

    // We calculate a shifted intersection from the original intersection using the inverse of the exponential
    // and the provided reference
    const double normalised_J         = J_axis_intersect / J_intersection_reference;
    const double shifted_intersection = J_intersection_reference * std::pow(normalised_J, inv_gamma);

    // Now we find the M intersection of two lines
    // line from origin to J,M Max       l1(x) = J/M * x
    // line from J Intersect' with slope l2(x) = slope * x + Intersect'

    //return shifted_intersection / ((J_max / M_max) - slope);
    return shifted_intersection * M_max / (J_max - slope * M_max);
}

double find_gamut_boundary_intersection(const f2 &JM_cusp, double J_max, double gamma_top_inv, double gamma_bottom_inv, const double J_intersect_source, const double slope, const double J_intersect_cusp)
{
    const double M_boundary_lower = estimate_line_and_boundary_intersection_M(J_intersect_source, slope, gamma_bottom_inv, JM_cusp[0], JM_cusp[1], J_intersect_cusp);

    // The upper hull is flipped and thus 'zeroed' at J_max
    // Also note we negate the slope
    const double f_J_intersect_cusp   = J_max - J_intersect_cusp;
    const double f_J_intersect_source = J_max - J_intersect_source;
    const double f_JM_cusp_J          = J_max - JM_cusp[0];
    const double M_boundary_upper =
      estimate_line_and_boundary_intersection_M(f_J_intersect_source, -slope, gamma_top_inv, f_JM_cusp_J, JM_cusp[1], f_J_intersect_cusp);

    // Smooth minimum between the two calculated values for the M component
    const double M_boundary = smin_scaled(M_boundary_lower, M_boundary_upper, JM_cusp[1]);
    return M_boundary;
}

template <bool invert>
inline double reinhard_remap(const double scale, const double nd)
{
    if (invert)
    {
        if (nd >= 1.0f) // TODO: given remap_M already tests against proportion do we need this asymptote test
            return scale;
        else
            return scale * -(nd / (nd - 1.0f));
    }
    return scale * nd / (1.0f + nd);
}

template <bool invert>
inline double remap_M(const double M, const double gamut_boundary_M, const double reach_boundary_M)
{
    const double boundary_ratio = gamut_boundary_M / reach_boundary_M;
    const double proportion = std::max(boundary_ratio, compression_threshold);
    const double threshold  = proportion * gamut_boundary_M;

    if (M <= threshold || proportion >= 1.0f)
        return M;

    // Translate to place threshold at zero
    const double m_offset     = M - threshold;
    const double gamut_offset = gamut_boundary_M - threshold;
    const double reach_offset = reach_boundary_M - threshold;

    const double scale = reach_offset / ((reach_offset / gamut_offset) - 1.0f);
    const double nd = m_offset / scale;

    // shift back to absolute
    return threshold + reinhard_remap<invert>(scale, nd);
}

template <bool invert>
f3 compressGamut(const f3 &JMh, double Jx, const ResolvedSharedCompressionParameters& sr, const GamutCompressParams& p, const HueDependantGamutParams hdp)
{
    const double J = JMh[0];
    const double M = JMh[1];
    const double h = JMh[2];

    const double slope_gain = get_focus_gain(Jx, hdp.analytical_threshold, sr.limit_J_max, p.focus_dist);
    const double J_intersect_source = solve_J_intersect(J, M, hdp.focusJ, sr.limit_J_max, slope_gain);
    const double gamut_slope = compute_compression_vector_slope(J_intersect_source, hdp.focusJ, sr.limit_J_max, slope_gain);

    const double J_intersect_cusp = solve_J_intersect(hdp.JMcusp[0], hdp.JMcusp[1], hdp.focusJ, sr.limit_J_max, slope_gain);
    const double gamut_boundary_M = find_gamut_boundary_intersection(hdp.JMcusp, sr.limit_J_max, hdp.gamma_top_inv, hdp.gamma_bottom_inv, J_intersect_source, gamut_slope, J_intersect_cusp);

    if (gamut_boundary_M <= 0.0f) // TODO: when/why does this happen?
    {
        return {J, 0.f, h};
    }

    const double reachBoundaryM   = estimate_line_and_boundary_intersection_M(J_intersect_source, gamut_slope, sr.model_gamma_inv, sr.limit_J_max, sr.reachMaxM, sr.limit_J_max);

    const double remapped_M = remap_M<invert>(M, gamut_boundary_M, reachBoundaryM);

    const f3 JMcompressed {
        J_intersect_source + remapped_M * gamut_slope,
        remapped_M,
        h
    };
    return JMcompressed;
}

inline double compute_focusJ(double cusp_J, double mid_J, double limit_J_max)
{
    return Lerp(cusp_J, mid_J, std::min(1.0, cusp_mid_blend - (cusp_J / limit_J_max)));
}

HueDependantGamutParams init_HueDependantGamutParams(const double hue, const ResolvedSharedCompressionParameters &sr, const GamutCompressParams &p)
{
    HueDependantGamutParams hdp;
    hdp.gamma_bottom_inv = p.lower_hull_gamma_inv;

    const unsigned int i_hi = lookup_hue_interval(hue, p.hue_table, p.hue_linearity_search_range);
    const double t = interpolation_weight(hue, p.hue_table[i_hi-1], p.hue_table[i_hi]);
    const f3 cusp = cusp_from_table(i_hi, t, p.gamut_cusp_table);

    hdp.JMcusp = { cusp[0], cusp[1] };
    hdp.gamma_top_inv = { cusp[2] };
    hdp.focusJ = compute_focusJ(hdp.JMcusp[0], p.mid_J, sr.limit_J_max);
    hdp.analytical_threshold = Lerp(hdp.JMcusp[0], sr.limit_J_max, focus_gain_blend);
    return hdp;
}

f3 gamut_compress_fwd(const f3 &JMh, const ResolvedSharedCompressionParameters &sr, const GamutCompressParams &p)
{
    const double J = JMh[0];
    const double M = JMh[1];
    const double h = JMh[2];

    if (J <= 0.0f) // Limit to +ve J values // TODO test this is needed
    {
        return {0.0f, 0.f, h};
    }
    if (M <= 0.0f || J > sr.limit_J_max) // We compress M only so avoid mapping zero
                                         // Above the expected maximum we explicitly map to 0 M
    {
        return {J, 0.f, h};
    }
    const HueDependantGamutParams hdp = init_HueDependantGamutParams(h, sr, p);

    return compressGamut<false>(JMh, JMh[0], sr, p, hdp);
}

[[maybe_unused]] f3 gamut_compress_inv(const f3 &JMh, const ResolvedSharedCompressionParameters &sr, const GamutCompressParams &p)
{
    const double J = JMh[0];
    const double M = JMh[1];
    const double h = JMh[2];

    if (J <= 0.0f) // Limit to +ve J values // TODO test this is needed
    {
        return {0.0f, 0.f, h};
    }
    if (M <= 0.0f || J > sr.limit_J_max) // We compress M only so avoid mapping zero
                                         // Above the expected maximum we explicitly map to 0 M
    {
        return {J, 0.f, h};
    }
    const HueDependantGamutParams hdp = init_HueDependantGamutParams(h, sr, p);

    double Jx = J;
    if (Jx > hdp.analytical_threshold)
    {
        // Approximation above threshold
        Jx = compressGamut<true>(JMh, Jx, sr, p, hdp)[0];
    }
    return compressGamut<true>(JMh, Jx, sr, p, hdp);
}

static constexpr unsigned int gamma_test_count = 5;
struct testData {
    f3 testJMh;
    double J_intersect_source;
    double slope;
    double J_intersect_cusp;
};

std::array<testData, gamma_test_count> generate_gamma_test_data(const f2 &JMcusp, double hue, double limit_J_max, double mid_J, double focus_dist)
{
    const std::array<double, gamma_test_count> testPositions = {0.01f, 0.1f, 0.5f, 0.8f, 0.99f};
    std::array<testData, gamma_test_count> data;
    const double analytical_threshold = Lerp(JMcusp[0], limit_J_max, focus_gain_blend);
    const double focusJ = compute_focusJ(JMcusp[0], mid_J, limit_J_max);

    unsigned int testIndex = 0;
    std::generate(data.begin(), data.end(), [JMcusp, limit_J_max, focus_dist, testPositions, hue, analytical_threshold, focusJ, testIndex]() mutable {
        const double testJ = Lerp(JMcusp[0], limit_J_max, testPositions[testIndex]);
        const double slope_gain = get_focus_gain(testJ, analytical_threshold, limit_J_max, focus_dist);
        const double J_intersect_source = solve_J_intersect(testJ, JMcusp[1], focusJ, limit_J_max, slope_gain);
        testData result = {
            { testJ, JMcusp[1], hue },
            J_intersect_source,
            compute_compression_vector_slope(J_intersect_source, focusJ, limit_J_max, slope_gain),
            solve_J_intersect(JMcusp[0], JMcusp[1], focusJ, limit_J_max, slope_gain)
        };
        testIndex++;
        return result;
    });
    return data;
}

bool evaluate_gamma_fit(
    const f2 &JMcusp,
    const std::array<testData, gamma_test_count> data,
    double topGamma_inv,
    double peakLuminance,
    double limit_J_max,
    double lower_hull_gamma_inv,
    const JMhParams &limitJMhParams)
{
    const double luminance_limit = peakLuminance / reference_luminance;
    for (auto test_data: data)
    {
        const double approxLimit_M = find_gamut_boundary_intersection(JMcusp, limit_J_max, topGamma_inv, lower_hull_gamma_inv,
                                                    test_data.J_intersect_source, test_data.slope, test_data.J_intersect_cusp);
        const double approxLimit_J = test_data.J_intersect_source + test_data.slope * approxLimit_M;

        const f3 approximate_JMh = {approxLimit_J, approxLimit_M, test_data.testJMh[2]};
        const f3 newLimitRGB = JMh_to_RGB(approximate_JMh, limitJMhParams);

        if (!outside_hull(newLimitRGB, luminance_limit))
        {
            return false;
        }
    }

    return true;
}

void make_upper_hull_gamma(
    const Table1D &hue_table,
    Table3D &gamutCuspTable,
    double peakLuminance,
    double limit_J_max,
    double mid_J,
    double focus_dist,
    double lower_hull_gamma_inv,
    const JMhParams &limitJMhParams)
{
    for (unsigned int i = gamutCuspTable.first_nominal_index; i != gamutCuspTable.upper_wrap_index; ++i)
    {
        const double hue = hue_table[i];
        const f2 JMcusp = { gamutCuspTable[i][0], gamutCuspTable[i][1] };

        std::array<testData, gamma_test_count> data = generate_gamma_test_data(JMcusp, hue, limit_J_max, mid_J, focus_dist);

        // TODO: calculate best fitting inverse gamma directly rather than reciprocating it in the loop
        // TODO: adjacent found gamma values typically fall close to each other could initialise the search range
        //       with values near to speed up searching. Note that discrepancies do occur at/near corners of gamut
        const double search_range = gammaSearchStep;
        double low = gammaMinimum;
        double high = low + search_range;
        bool outside = false;

        auto gamma_fit_predicate = [JMcusp, data, peakLuminance, limit_J_max, lower_hull_gamma_inv, limitJMhParams](double gamma)
        {
            return evaluate_gamma_fit(JMcusp, data, 1.0f / gamma, peakLuminance, limit_J_max, lower_hull_gamma_inv, limitJMhParams);
        };
        while (!(outside) && (high < gammaMaximum))
        {
            const bool gammaFound = gamma_fit_predicate(high);
            if (!gammaFound)
            {
                low = high;
                high = high + search_range;
            }
            else
            {
                outside = true;
            }
        }

        double testGamma = -1.f;
        while ( (high-low) > gammaAccuracy)
        {
            testGamma = midpoint(high, low);
            const bool gammaFound = gamma_fit_predicate(testGamma);
            if (gammaFound)
            {
                high = testGamma;
            }
            else
            {
                low = testGamma;
            }
        }
        gamutCuspTable[i][2] = 1.0f / high;
    }

    // Copy last populated entries to empty spot 'wrapping' entries
    gamutCuspTable[gamutCuspTable.lower_wrap_index][2] = gamutCuspTable[gamutCuspTable.last_nominal_index][2];
    gamutCuspTable[gamutCuspTable.upper_wrap_index][2] = gamutCuspTable[gamutCuspTable.first_nominal_index][2];
}

// Tonescale pre-calculations
ToneScaleParams init_ToneScaleParams(double peakLuminance)
{
    // Preset constants that set the desired behavior for the curve
    const double n = peakLuminance;

    const double n_r = 100.0f;    // normalized white in nits (what 1.0 should be)
    const double g = 1.15f;       // surround / contrast
    const double c = 0.18f;       // anchor for 18% grey
    const double c_d = 10.013f;   // output luminance of 18% grey (in nits)
    const double w_g = 0.14f;     // change in grey between different peak luminance
    const double t_1 = 0.04f;     // shadow toe or flare/glare compensation
    const double r_hit_min = 128.f;   // scene-referred value "hitting the roof"
    const double r_hit_max = 896.f;   // scene-referred value "hitting the roof"

    // Calculate output constants
    // TODO: factor these calculations into well named functions
    // TODO: ensure correct use of n_r vs n vs reference_luminance vs 100.f etc
    const double r_hit = r_hit_min + (r_hit_max - r_hit_min) * (log(n/n_r)/log(10000.f/100.f));
    const double m_0 = (n / n_r);
    const double m_1 = 0.5f * (m_0 + sqrt(m_0 * (m_0 + 4.f * t_1)));
    const double u = std::pow((r_hit/m_1)/((r_hit/m_1)+1.f),g);
    const double m = m_1 / u;
    const double w_i = log(n/100.f)/log(2.f);
    const double c_t = c_d/n_r * (1.f + w_i * w_g);
    const double g_ip = 0.5f * (c_t + sqrt(c_t * (c_t + 4.f * t_1)));
    const double g_ipp2 = -(m_1 * std::pow((g_ip/m),(1.f/g))) / (std::pow(g_ip/m , 1.f/g)-1.f);
    const double w_2 = c / g_ipp2;
    const double s_2 = w_2 * m_1 * reference_luminance;
    const double u_2 = std::pow((r_hit/m_1)/((r_hit/m_1) + w_2), g);
    const double m_2 = m_1 / u_2;
    const double inverse_limit = n / (u_2 * n_r);
    const double forward_limit = 8.0f * r_hit;
    const double log_peak = log10( n / n_r);

    ToneScaleParams TonescaleParams = {
        n,
        n_r,
        g,
        t_1,
        c_t,
        s_2,
        u_2,
        m_2,
        forward_limit,
        inverse_limit,
        log_peak
    };

    return TonescaleParams;
}

SharedCompressionParameters init_SharedCompressionParams(double peakLuminance, const JMhParams &inputJMhParams, const JMhParams &reachParams)
{
    const double limit_J_max = Y_to_J(peakLuminance, inputJMhParams);
    const double model_gamma_inv = 1.f / model_gamma();

    SharedCompressionParameters params = {
        limit_J_max,
        model_gamma_inv,
        make_reach_m_table(reachParams, limit_J_max)
    };
    return params;
}

ResolvedSharedCompressionParameters resolve_CompressionParams(double hue, const SharedCompressionParameters &p)
{
    ResolvedSharedCompressionParameters params = {
        p.limit_J_max,
        p.model_gamma_inv,
        reach_m_from_table(hue, p.reach_m_table)
    };
    return params;
}

ChromaCompressParams init_ChromaCompressParams(double peakLuminance, const ToneScaleParams &tsParams)
{
    // Calculated chroma compress variables
    // TODO: name these magic constants
    const double compr = chroma_compress + (chroma_compress * chroma_compress_fact) * tsParams.log_peak;
    const double sat = std::max(0.2, chroma_expand - (chroma_expand * chroma_expand_fact) * tsParams.log_peak);
    const double sat_thr = chroma_expand_thr / tsParams.n;
    const double chroma_compress_scale =  std::pow(0.03379f * peakLuminance, 0.30596f) - 0.45135f;

    ChromaCompressParams params = {
        sat,
        sat_thr,
        compr,
        chroma_compress_scale
    };
    return params;
}

std::array<int, 2> determine_hue_linearity_search_range(const Table3D &gamutCuspTable)
{
    // This function searches through the hues looking for the largest deviations from a linear distribution
    // We can then use this to initialise the binary search range to something smaller than the full one to
    // reduce the number of lookups per hue lookup from ~ceil(log2(table size)) to ~ceil(log2(range))
    // during image rendering.

    // TODO: Padding values are a quick hack to ensure the range encloses the needed range, left as an exersize
    // for the reader to fully reason if these values could be smaller, probably best done the closer the hue
    // distribution comes to linear as the overhead becomes a potentially greater issue
    constexpr int lower_padding = 0;
    constexpr int upper_padding = 1;
    std::array<int, 2> hue_linearity_search_range = {lower_padding, upper_padding};
    for (unsigned int i = gamutCuspTable.first_nominal_index; i != gamutCuspTable.upper_wrap_index; ++i)
    {
        const unsigned int pos = gamutCuspTable.nominal_hue_position_in_uniform_table(gamutCuspTable[i][2]); // TODO: compute from hue table less cache pressure?
        const int delta = int(i) - int(pos);
        hue_linearity_search_range[0] = std::min(hue_linearity_search_range[0], delta + lower_padding);
        hue_linearity_search_range[1] = std::max(hue_linearity_search_range[1], delta + upper_padding);

        //std::cout << i << " " << pos << " " << delta << " " << gamutCuspTable[i][0] << " " << gamutCuspTable[i][1] << " " << gamutCuspTable[i][2] << "\n";
    }
    //std::cout << "search range " << hue_linearity_search_range[0] << " " << hue_linearity_search_range[1] << "\n";
    return hue_linearity_search_range;
}

GamutCompressParams init_GamutCompressParams(double peakLuminance, const JMhParams &inputJMhParams, const JMhParams &limitJMhParams,
                                             const ToneScaleParams &tsParams, const SharedCompressionParameters &shParams, const JMhParams &reachParams)
{
    double mid_J = Y_to_J(tsParams.c_t * reference_luminance, inputJMhParams);

    // Calculated chroma compress variables
    const double focus_dist = focus_distance + focus_distance * focus_distance_scaling * tsParams.log_peak;
    const double lower_hull_gamma_inv =  1.0f / (1.14f + 0.07f * tsParams.log_peak); // TODO: name these magic constants

    GamutCompressParams params;
    params.mid_J = mid_J;
    params.focus_dist = focus_dist;
    params.lower_hull_gamma_inv = lower_hull_gamma_inv;
    params.gamut_cusp_table = make_uniform_hue_gamut_table(reachParams, limitJMhParams, peakLuminance, tsParams.forward_limit, shParams, params.hue_table);
    params.hue_linearity_search_range = determine_hue_linearity_search_range(params.gamut_cusp_table);
    make_upper_hull_gamma(params.hue_table, params.gamut_cusp_table, peakLuminance, shParams.limit_J_max, mid_J, focus_dist,
                          lower_hull_gamma_inv, limitJMhParams); //TODO: mess of parameters
    return params;
}

struct AcesConfiguration
{
    JMhParams Input;
    JMhParams Output;
    ToneScaleParams ToneScale;
    SharedCompressionParameters Shared;
    ChromaCompressParams Chroma;
    GamutCompressParams Gamut;
};

AcesConfiguration MakeConfiguration(double PeakLuminance, const Primaries& OutputPrimaries)
{
    AcesConfiguration Result;
    Result.Input = init_JMhParams(AcesAp0);
    Result.Output = init_JMhParams(OutputPrimaries);
    Result.ToneScale = init_ToneScaleParams(PeakLuminance);
    const JMhParams Reach = init_JMhParams(AcesAp1);
    Result.Shared = init_SharedCompressionParams(
        PeakLuminance, Result.Input, Reach);
    Result.Chroma = init_ChromaCompressParams(PeakLuminance, Result.ToneScale);
    Result.Gamut = init_GamutCompressParams(
        PeakLuminance, Result.Input, Result.Output, Result.ToneScale,
        Result.Shared, Reach);
    return Result;
}

const AcesConfiguration& ResolveConfiguration(
    double PeakLuminance, EAces2ReferenceGamut Gamut)
{
    static const AcesConfiguration Rec709At1000 =
        MakeConfiguration(1000.0, Rec709);
    static const AcesConfiguration Rec709At2000 =
        MakeConfiguration(2000.0, Rec709);
    static const AcesConfiguration Rec2020At1000 =
        MakeConfiguration(1000.0, Rec2020);
    static const AcesConfiguration Rec2020At2000 =
        MakeConfiguration(2000.0, Rec2020);

    if (Gamut == EAces2ReferenceGamut::Rec709D65)
    {
        return PeakLuminance == 1000.0 ? Rec709At1000 : Rec709At2000;
    }
    return PeakLuminance == 1000.0 ? Rec2020At1000 : Rec2020At2000;
}

} // namespace

std::array<double, 3> ApplyAces2OutputTransform(
    const std::array<double, 3>& Aces2065_1,
    double PeakLuminanceNits,
    EAces2ReferenceGamut LimitingGamut) noexcept
{
    const AcesConfiguration& Configuration =
        ResolveConfiguration(PeakLuminanceNits, LimitingGamut);

    const m33f Ap0ToAp1 = RGBtoRGB_f33(AcesAp0, AcesAp1);
    const m33f Ap1ToAp0 = invert_f33(Ap0ToAp1);
    f3 Ap1 = mult_f3_f33(Aces2065_1, Ap0ToAp1);
    const double UpperBound = 8.0 *
        (128.0 + 768.0 *
            (std::log(PeakLuminanceNits / 100.0) /
                std::log(10000.0 / 100.0)));
    for (double& Component : Ap1)
    {
        Component = std::clamp(Component, 0.0, UpperBound);
    }
    const f3 ClampedAp0 = mult_f3_f33(Ap1, Ap1ToAp0);

    const f3 Aab = RGB_to_Aab(ClampedAp0, Configuration.Input);
    const f3 JMh = Aab_to_JMh(Aab, Configuration.Input);
    const ResolvedSharedCompressionParameters Resolved =
        resolve_CompressionParams(JMh[2], Configuration.Shared);
    const double HueRadians = to_radians(JMh[2]);
    const double CosHue = std::cos(HueRadians);
    const double SinHue = std::sin(HueRadians);
    const double MNorm = chroma_compress_norm(
        CosHue, SinHue, Configuration.Chroma.chroma_compress_scale);
    const double ToneScaledJ = tonescale_A_to_J_fwd(
        Aab[0], Configuration.Input, Configuration.ToneScale);
    const f3 ToneMapped = chroma_compress_fwd(
        JMh, ToneScaledJ, MNorm, Resolved, Configuration.Chroma);
    const f3 Compressed =
        gamut_compress_fwd(ToneMapped, Resolved, Configuration.Gamut);
    const f3 OutputAab = JMh_to_Aab(
        Compressed, CosHue, SinHue, Configuration.Output);
    f3 Output = Aab_to_RGB(OutputAab, Configuration.Output);

    const double NormalizedPeak = PeakLuminanceNits / reference_luminance;
    for (double& Component : Output)
    {
        Component = std::clamp(Component, 0.0, NormalizedPeak) *
            reference_luminance;
    }
    return Output;
}

} // namespace Stoner::Renderer::Private
