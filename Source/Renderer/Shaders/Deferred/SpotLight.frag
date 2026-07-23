#version 450

layout(location = 0) flat in uint LightIndex;

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

vec3 ReconstructWorldPosition(vec2 uv, float depth)
{
    vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 world = Frame.InverseViewProjection * clip;
    return world.xyz / world.w;
}

void main()
{
    vec2 uv = gl_FragCoord.xy * Frame.OutputExtent.zw;
    vec3 worldPosition = ReconstructWorldPosition(uv, texture(DepthTexture, uv).r);
    LightRecord light = LightData.Items[LightIndex];
    vec3 fromLight = worldPosition - light.PositionRange.xyz;
    float distanceToLight = length(fromLight);
    float coneCos = dot(normalize(fromLight), normalize(light.DirectionOuterCos.xyz));
    if (distanceToLight > light.PositionRange.w || coneCos < light.DirectionOuterCos.w)
    {
        discard;
    }
    float cone = smoothstep(light.DirectionOuterCos.w,
        light.InnerCosTypeVolumeMode.x, coneCos);
    vec3 normal = normalize(texture(NormalRoughnessTexture, uv).xyz);
    float attenuation = pow(max(1.0 - distanceToLight / light.PositionRange.w, 0.0), 2.0);
    float diffuse = max(dot(normal, normalize(-fromLight)), 0.0);
    OutLighting = vec4(light.ColorIntensity.rgb * light.ColorIntensity.a *
        attenuation * cone * diffuse, 0.0);
}
