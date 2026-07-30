#version 450

layout(location = 0) in vec3 WorldNormal;

layout(set = 1, binding = 0, std140) uniform DrawMaterial
{
    mat4 Model;
    mat4 WorldNormalFromModel;
    vec4 BaseColorAO;
    vec4 EmissiveMetallic;
    vec4 RoughnessAlphaCutoffFlags;
} Draw;

layout(location = 0) out vec4 OutBaseColorAO;
layout(location = 1) out vec4 OutNormalRoughness;
layout(location = 2) out vec4 OutEmissiveMetallic;

void main()
{
    if (Draw.RoughnessAlphaCutoffFlags.w > 0.5 &&
        Draw.RoughnessAlphaCutoffFlags.y < Draw.RoughnessAlphaCutoffFlags.z)
    {
        discard;
    }
    OutBaseColorAO = vec4(Draw.BaseColorAO.rgb, Draw.BaseColorAO.a);
    OutNormalRoughness = vec4(normalize(WorldNormal), Draw.RoughnessAlphaCutoffFlags.x);
    OutEmissiveMetallic = Draw.EmissiveMetallic;
}
