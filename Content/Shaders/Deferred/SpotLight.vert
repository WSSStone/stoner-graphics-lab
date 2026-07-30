#version 450

layout(location = 0) in vec3 InPosition;

layout(set = 0, binding = 0, std140) uniform FrameView
{
    mat4 View;
    mat4 Projection;
    mat4 InverseViewProjection;
    mat4 ViewProjection;
    vec4 CameraPosition;
    vec4 OutputExtent;
    vec4 DepthConvention;
} Frame;

struct LightRecord
{
    vec4 PositionRange;
    vec4 DirectionOuterCos;
    vec4 ColorIntensity;
    vec4 InnerCosTypeVolumeMode;
};
layout(set = 3, binding = 0, std430) readonly buffer Lights { LightRecord Items[]; } LightData;

layout(location = 0) flat out uint LightIndex;

void main()
{
    LightIndex = gl_InstanceIndex;
    LightRecord light = LightData.Items[LightIndex];
    vec3 worldPosition = light.PositionRange.xyz + InPosition * light.PositionRange.w;
    gl_Position = Frame.ViewProjection * vec4(worldPosition, 1.0);
}
