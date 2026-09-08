#version 450

layout(location = 0) in vec2 UV;
layout(location = 1) in vec4 LinearVertexColor;
layout(location = 0) out vec4 OutColor;
layout(set = 0, binding = 0) uniform sampler2D UITexture;
layout(set = 0, binding = 1, std140) uniform UIParameters
{
    vec4 ClipScaleTranslate;
    vec4 Display;
} Parameters;

void main()
{
    // SRGBRec709 uses an sRGB sampled format: texels decode before filtering.
    // LinearRec709 and AlphaCoverage use linear storage; neither is decoded here.
    vec4 sampled = texture(UITexture, UV);
    vec3 color = LinearVertexColor.rgb * sampled.rgb;
    if (Parameters.Display.y > 0.5)
    {
        color = vec3(
            dot(color, vec3(0.62740389593469903, 0.32928303837788381, 0.04331306568741722)),
            dot(color, vec3(0.06909728935823199, 0.91954039507545904, 0.01136231556630916)),
            dot(color, vec3(0.01639143887515023, 0.08801330787722578, 0.89559525324762401)));
    }
    // Native source-alpha blending applies coverage once. RGB-only writes
    // preserve the composition target's alpha initialized to one by UICopy.
    OutColor = vec4(color * Parameters.Display.x, LinearVertexColor.a * sampled.a);
}
