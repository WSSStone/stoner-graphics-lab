#version 450

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 OutColor;
layout(set = 0, binding = 0) uniform sampler2D InputColorTexture;
layout(set = 0, binding = 1, std140) uniform DiagnosticParameters
{
    vec4 Range; // minimum, maximum, reserved, reserved, in selected-stage units
} Parameters;

void main()
{
    vec3 color = texture(InputColorTexture, UV).rgb;
    if (any(isnan(color)) || any(isinf(color)))
    {
        OutColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }
    float minimum = Parameters.Range.x;
    float maximum = Parameters.Range.y;
    float scale = max(1.0, max(abs(minimum), abs(maximum)));
    float span = maximum / scale - minimum / scale;
    if (!(span > 0.0))
    {
        OutColor = vec4(1.0, 0.0, 1.0, 1.0);
        return;
    }
    color = (clamp(color, minimum, maximum) / scale - minimum / scale) / span;
    // This pass writes linear diagnostic channel values into an sRGB target.
    // Attachment encoding and later sampled-sRGB decoding each happen once.
    // No scene exposure, viewing transform or output-device transfer is applied.
    OutColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
