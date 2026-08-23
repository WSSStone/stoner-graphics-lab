#version 450

layout(location = 0) in vec3 WorldNormal;
layout(location = 1) in vec2 TexCoord0;
layout(location = 2) in vec4 WorldTangent;

layout(set = 1, binding = 0, std140) uniform DrawMaterial
{
    mat4 Model;
    mat4 WorldNormalFromModel;
    vec4 BaseColorAO;
    vec4 EmissiveMetallic;
    vec4 RoughnessAlphaCutoffFlags;
} Draw;

layout(set = 1, binding = 1) uniform sampler2D BaseColorTexture;
layout(set = 1, binding = 2) uniform sampler2D MetallicRoughnessTexture;
layout(set = 1, binding = 3) uniform sampler2D NormalTexture;
layout(set = 1, binding = 4) uniform sampler2D OcclusionTexture;
layout(set = 1, binding = 5) uniform sampler2D EmissiveTexture;

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
    vec4 sampledBaseColor = texture(BaseColorTexture, TexCoord0);
    vec4 sampledMetallicRoughness = texture(MetallicRoughnessTexture, TexCoord0);
    vec3 tangentNormal = texture(NormalTexture, TexCoord0).xyz * 2.0 - 1.0;
    vec3 geometricNormal = normalize(WorldNormal);
    vec3 tangent = normalize(WorldTangent.xyz -
        geometricNormal * dot(geometricNormal, WorldTangent.xyz));
    vec3 bitangent = normalize(cross(geometricNormal, tangent)) * WorldTangent.w;
    vec3 worldNormal = normalize(mat3(tangent, bitangent, geometricNormal) *
        tangentNormal);
    float occlusion = texture(OcclusionTexture, TexCoord0).r;
    vec3 emissive = Draw.EmissiveMetallic.rgb *
        texture(EmissiveTexture, TexCoord0).rgb;
    OutBaseColorAO = vec4(
        Draw.BaseColorAO.rgb * sampledBaseColor.rgb,
        Draw.BaseColorAO.a * occlusion);
    OutNormalRoughness = vec4(worldNormal,
        Draw.RoughnessAlphaCutoffFlags.x * sampledMetallicRoughness.g);
    OutEmissiveMetallic = vec4(emissive,
        Draw.EmissiveMetallic.a * sampledMetallicRoughness.b);
}
