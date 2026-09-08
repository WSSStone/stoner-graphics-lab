#version 450

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 2) in vec4 PackedColor;
layout(location = 0) out vec2 UV;
layout(location = 1) out vec4 LinearVertexColor;

layout(set = 0, binding = 1, std140) uniform UIParameters
{
    vec4 ClipScaleTranslate;
    // x: effective UI unit white, y: Rec.2020 destination flag, zw: reserved.
    vec4 Display;
} Parameters;

vec3 decodeSrgb(vec3 encoded)
{
    return mix(pow((encoded + 0.055) / 1.055, vec3(2.4)),
        encoded / 12.92, lessThanEqual(encoded, vec3(0.04045)));
}

void main()
{
    gl_Position = vec4(Position * Parameters.ClipScaleTranslate.xy +
        Parameters.ClipScaleTranslate.zw, 0.0, 1.0);
    UV = TexCoord;
    // Decode before rasterizer interpolation. Coverage remains linear.
    LinearVertexColor = vec4(decodeSrgb(PackedColor.rgb), PackedColor.a);
}
