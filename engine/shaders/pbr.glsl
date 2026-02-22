#include "consts.glsl"

float trowbridgeReitzGgx(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float schlickGgx(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float smithGeometry(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = schlickGgx(NdotV, roughness);
    float ggx1  = schlickGgx(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

#define ndf trowbridgeReitzGgx
#define geometry
#define fresnel fresnelShlick

float cookTorranceBDRF()
{
    return 0.f;
}

vec3 pbrLight(vec3 lightDir, float intensity, vec3 cameraDir, vec3 n, float shadow, vec3 albedo,vec2 metallicRoughness, vec3 f0)
{
    vec3 L = lightDir;
    vec3 H = normalize(cameraDir + L);
    vec3 radiance     = vec3(1.f - shadow) * intensity;

    // cook-torrance brdf
    float NDF = trowbridgeReitzGgx(n, H, metallicRoughness.y);
    float G   = smithGeometry(n, cameraDir, L, metallicRoughness.y);
    vec3 F    = fresnelSchlick(max(dot(H, cameraDir), 0.0), f0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallicRoughness.x;

    vec3 numerator    = NDF * G * F;
    float denominator = 4.0 * max(dot(n, cameraDir), 0.0) * max(dot(n, L), 0.0) + 0.0001;
    vec3 specular     = numerator / denominator;

    // add to outgoing radiance Lo
    float NdotL = max(dot(n, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}
