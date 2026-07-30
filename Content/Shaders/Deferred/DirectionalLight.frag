#version 450

layout(location = 0) in vec2 UV;
layout(location = 1) flat in uint LightIndex;

layout(set = 2, binding = 0) uniform sampler2D BaseColorAOTexture;
layout(set = 2, binding = 1) uniform sampler2D NormalRoughnessTexture;
layout(set = 2, binding = 2) uniform sampler2D EmissiveMetallicTexture;
layout(set = 2, binding = 3) uniform sampler2D DepthTexture;

struct LightRecord
{
    vec4 PositionRange;
    vec4 DirectionOuterCos;
    vec4 ColorIntensity;
    vec4 InnerCosTypeVolumeMode;
};
layout(set = 3, binding = 0, std430) readonly buffer Lights { LightRecord Items[]; } LightData;

layout(location = 0) out vec4 OutLighting;

void main()
{
    vec3 normal = normalize(texture(NormalRoughnessTexture, UV).xyz);
    vec3 lightDirection = normalize(-LightData.Items[LightIndex].DirectionOuterCos.xyz);
    float diffuse = max(dot(normal, lightDirection), 0.0);
    OutLighting = vec4(LightData.Items[LightIndex].ColorIntensity.rgb *
        LightData.Items[LightIndex].ColorIntensity.a * diffuse, 0.0);
}
