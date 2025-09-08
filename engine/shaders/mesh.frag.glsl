#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "debug_utils.glsl"
#include "lights.glsl"
#include "utils.glsl"
#include "pbr.glsl"
#include "scene.glsl"
#include "mesh.glsl"

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(buffer_reference, std430) readonly buffer ShadowPassData
{ 
	mat4 lightViewProj[4];
	mat4 invLightViewProj[4];
	vec4 cascadeDistances[4];
	int cascadeCount;
};

layout(push_constant) uniform Constants
{	
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
	ShadowPassData shadowData;
	Lights lights;
	LightIds lightIds;
	LightTileData lightTiles;
	int shadowMapIndex;
} constants;


layout (location = 4) in vec2 vert_uv;
layout (location = 5) in flat int index;
layout (location = 6) in vec3 viewPos;
layout (location = 7) in vec3 pos;
layout (location = 8) in mat3 tbn;
layout (location = 3) in vec3 tangentCameraPos;
layout (location = 2) in vec3 tangentFragPos;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 reflection;

#include "parallax.glsl"

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0
);

#define PCF_TAPS_PER_AXIS 15

float shadowIntensity(mat4 viewProj, uint cascadeIndex, float cascadeCount)
{
    vec4 lightSpacePos = viewProj * vec4(pos, 1.f);
    lightSpacePos = lightSpacePos / lightSpacePos.w;
    vec2 lightSpacePosNDC = lightSpacePos.xy * 0.5f + 0.5f;

    float shadow = 0.f;

    // Add directional bias
	float bias = 0.01f;

    float cascadeURange = 1.f / cascadeCount;
	vec2 cascadeNDC = lightSpacePosNDC.xy * vec2(cascadeURange, 1.f) + vec2(cascadeURange * cascadeIndex, 0.f);

    //float shadowDepth = texture(textures[constants.shadowMapIndex], cascadeNDC).r;
    //if (shadowDepth < (lightSpacePos.z - bias))
    //{
    //    shadow = 0.85f;
    //}

    vec2 texelSize = 1.f / textureSize(textures[constants.shadowMapIndex], 0) / vec2(cascadeCount, 1.f);
    for (int x = -PCF_TAPS_PER_AXIS / 2; x <= PCF_TAPS_PER_AXIS / 2; x++)
    {
        for (int y = -PCF_TAPS_PER_AXIS / 2; y <= PCF_TAPS_PER_AXIS / 2; y++)
        {
            float shadowDepth = texture(textures[constants.shadowMapIndex], cascadeNDC + vec2(x, y) * texelSize).r;
            shadow += (lightSpacePos.z - bias) > shadowDepth ? 1.f : 0.f;
        }
    }
    shadow = shadow / float(PCF_TAPS_PER_AXIS * PCF_TAPS_PER_AXIS);
    //shadow = shadow / 121.f;

    return shadow;
}

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir, float cosTheta) {
    return color * cosTheta;
}

vec3 calculateSpecular(vec3 color, vec3 normal, vec3 lightDir, vec3 cameraDir, float cosTheta, float specularExp) {
    float specularIntensity = pow(max(0.f, dot(cameraDir, reflect(-lightDir, normal))), specularExp);
    return color * specularIntensity;
}

void main()
{
    const vec4 textureIndices = constants.modelData.data[index].textures;
    vec2 uv = parallaxOcclussionMapBinarySearch(vert_uv, int(textureIndices.z));

    vec3 albedo = texture(textures[int(textureIndices.x)], uv).rgb;
    vec3 texNormal = texture(textures[int(textureIndices.y)], uv).rgb;
    vec2 metallicRoughness = texture(textures[int(textureIndices.w)], uv).rg;

    vec3 f0 = vec3(0.04);
    f0 = mix(f0, albedo, metallicRoughness.r);

	uint cascadeIndex = 0;
	for(uint i = 0; i < constants.shadowData.cascadeCount - 1; ++i) {
		if(viewPos.z < constants.shadowData.cascadeDistances[i].x) {
			cascadeIndex = i + 1;
		}
	}

	float shadow = shadowIntensity(constants.shadowData.lightViewProj[cascadeIndex], cascadeIndex, float(constants.shadowData.cascadeCount));

	vec3 n = normalize(tbn * (texNormal * vec3(2.f) - vec3(1.f)));

    vec3 cameraDir = normalize(scene.cameraPos.xyz - pos);

    // Look up light tile
    vec2 fragmentPos = gl_FragCoord.xy / vec2(1920.f, 1080.f);
    vec2 lightTileId = fragmentPos * vec2(96.f, 54.f);
    int tileIndex = int(lightTileId.y) * 96 + int(lightTileId.x);
    uint lightCount = constants.lightTiles.tiles[tileIndex].count;
    uint lightOffset = constants.lightTiles.tiles[tileIndex].offset;

    vec3 Lo = vec3(0.0);
    for (uint i = 0; i < lightCount; i++)
    {
        uint lightIndex = constants.lightIds.ids[i + lightOffset];
        PointLight light = constants.lights.pointLights[lightIndex];
        vec3 lightDir = light.pos.xyz - pos;
        vec3 normLightDir = normalize(lightDir);
        float dist = length(lightDir);
        float radius = light.range.x;

        float normalizedDist = (radius - dist) / radius;
        float strength = pow(max(normalizedDist, 0.f), 2.f);

        vec3 L = normLightDir;
        vec3 H = normalize(cameraDir + L);
        //float attenuation = 1.0 / (dist);
        float attenuation = strength;
        vec3 radiance     = light.color.rgb * attenuation;

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // Global directional light
    {
        vec3 L = vec3(-scene.lightDir.x, -scene.lightDir.y, scene.lightDir.z);
        vec3 H = normalize(cameraDir + L);
        vec3 radiance     = vec3(1.f - shadow);

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
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.02) * albedo;
    vec3 color = ambient + Lo;

    if (constants.modelData.data[index].selected.x > 0.f)
    {
        color = mix(color, vec3(1.f, 1.f, 1.f), cos(scene.time.x * 5.f) * 0.5f + 0.5f);
    }

    //outColor = vec4(color * heatmapGradient(float(lightCount) / 32.f), 1.f);
    outColor = vec4(color, 1.f);
}
