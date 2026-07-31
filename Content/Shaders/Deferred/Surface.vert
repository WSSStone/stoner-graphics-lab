#version 450

// Frame and draw matrices are supplied as GLSL column-major payloads by the
// Renderer row-major packing contract.

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InNormal;

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

layout(set = 1, binding = 0, std140) uniform DrawMaterial
{
    mat4 Model;
    mat4 WorldNormalFromModel;
    vec4 BaseColorAO;
    vec4 EmissiveMetallic;
    vec4 RoughnessAlphaCutoffFlags;
} Draw;

layout(location = 0) out vec3 WorldNormal;

void main()
{
    gl_Position = Frame.ViewProjection * Draw.Model * vec4(InPosition, 1.0);
    WorldNormal = normalize(mat3(Draw.WorldNormalFromModel) * InNormal);
}
