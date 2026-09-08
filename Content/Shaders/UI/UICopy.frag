#version 450

layout(location = 0) in vec2 UV;
layout(location = 0) out vec4 OutColor;
layout(set = 0, binding = 0) uniform sampler2D InputColorTexture;

void main()
{
    // Copy the existing display-linear scene. The sole output transfer follows UI.
    OutColor = vec4(texture(InputColorTexture, UV).rgb, 1.0);
}
