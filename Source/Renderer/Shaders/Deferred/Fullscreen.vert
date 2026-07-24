#version 450

layout(location = 0) in vec2 InPosition;
layout(location = 0) out vec2 UV;
layout(location = 1) flat out uint LightIndex;

void main()
{
    gl_Position = vec4(InPosition, 0.0, 1.0);
    UV = InPosition * 0.5 + 0.5;
    LightIndex = gl_InstanceIndex;
}
