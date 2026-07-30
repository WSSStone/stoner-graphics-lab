#version 450

layout(location = 0) in vec2 UV;
layout(set = 2, binding = 0) uniform sampler2D BaseColorAOTexture;
layout(set = 2, binding = 2) uniform sampler2D EmissiveMetallicTexture;
layout(set = 2, binding = 4) uniform sampler2D LightingAccumulationTexture;

layout(location = 0) out vec4 OutColor;

void main()
{
    vec4 baseAO = texture(BaseColorAOTexture, UV);
    vec3 emissive = texture(EmissiveMetallicTexture, UV).rgb;
    vec3 directLighting = texture(LightingAccumulationTexture, UV).rgb;
    vec3 linearColor = baseAO.rgb * directLighting + emissive;
    OutColor = vec4(linearColor, 1.0);
}
