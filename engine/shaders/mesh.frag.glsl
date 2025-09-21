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

#include "bindless.glsl"

layout(buffer_reference, std430) readonly buffer ShadowPassData
{ 
	mat4 lightViewProj[4];
	mat4 invLightViewProj[4];
	vec4 cascadeDistances[4];
	int cascadeCount;
};

layout(push_constant) uniform Constants
{
	vec4 enabledFeatures; // normal mapping, parallax mapping
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
	ShadowPassData shadowData;
	Lights lights;
	LightIds lightIds;
	LightTileData lightTiles;
	int shadowMapIndex;
	int depthMapIndex;
} constants;

layout (location = 4) in vec2 vert_uv;
layout (location = 5) in flat int index;
layout (location = 6) in vec3 viewPos;
layout (location = 7) in vec3 pos;
layout (location = 8) in mat3 tbn;
layout (location = 3) in vec3 tangentCameraPos;
layout (location = 2) in vec3 tangentFragPos;

layout (location = 0) out vec4 outColor;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outPos;
layout (location = 3) out vec4 outReflection;

#include "parallax.glsl"

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

    return shadow;
}

void main()
{
    const vec4 textureIndices = constants.modelData.data[index].textures;

    const bool parallaxMappingEnabled = constants.enabledFeatures.y != 0.f;
    vec2 uv = parallaxMappingEnabled ? parallaxOcclusionMapBinarySearch(vert_uv, int(textureIndices.z)) : vert_uv;
    //vec2 uv = vert_uv;

    vec3 albedo = texture(textures[int(textureIndices.x)], uv).rgb;
    vec3 texNormal = texture(textures[int(textureIndices.y)], uv).rgb;

    vec2 metallicRoughnessFactors = constants.modelData.data[index].metallicRoughnessFactors.rg;
    vec2 metallicRoughness = texture(textures[int(textureIndices.w)], uv).bg;
    metallicRoughness = clamp(vec2(0.f), vec2(1.f), metallicRoughness * metallicRoughnessFactors);

    vec3 f0 = vec3(0.04);
    f0 = mix(f0, albedo, metallicRoughness.r);

	uint cascadeIndex = 0;
	for(uint i = 0; i < constants.shadowData.cascadeCount - 1; ++i) {
		if(viewPos.z < constants.shadowData.cascadeDistances[i].x) {
			cascadeIndex = i + 1;
		}
	}

	float shadow = shadowIntensity(constants.shadowData.lightViewProj[cascadeIndex], cascadeIndex, float(constants.shadowData.cascadeCount));

    const bool normalMappingEnabled = constants.enabledFeatures.x != 0.f;
	vec3 n = normalMappingEnabled ? normalize(tbn * (texNormal * vec3(2.f) - vec3(1.f))) : tbn[2];
	outNormal = vec4(n, 1.f);

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
        float attenuation = pow(max(normalizedDist, 0.f), 2.f);
        float strength = light.range.y;

        vec3 L = normLightDir;
        vec3 H = normalize(cameraDir + L);
        vec3 radiance     = light.color.rgb * attenuation * strength;

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
        vec3 L = vec3(-scene.lightDirIntensity.x, -scene.lightDirIntensity.y, scene.lightDirIntensity.z);
        vec3 H = normalize(cameraDir + L);
        vec3 radiance     = vec3(1.f - shadow) * scene.lightDirIntensity.w;

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

    // Selection highlight
    if (constants.modelData.data[index].selected.x > 0.f)
    {
        color = mix(color, vec3(1.f, 1.f, 1.f), cos(scene.time.x * 5.f) * 0.5f + 0.5f);
    }

    // SSR v2.0
    {
        const mat4 mvp = scene.proj * scene.view;
        const float maxDist = 10.f;
        const int maxStepCount = 64;
        const float thickness = 0.005;

        const vec3 toFrag = -cameraDir;
        const vec3 reflectionDir = normalize(reflect(toFrag, n));

        const vec4 startWS = vec4(pos, 1.f);
        const vec4 startVS = scene.view * startWS;
        const vec4 startCS = scene.proj * startVS;
        const vec4 endWS = vec4(startWS.xyz + reflectionDir * maxDist, 1.f);
        const vec4 endVS = scene.view * endWS;
        const vec4 endCS = scene.proj * endVS;

        const float k0 = 1.f / startCS.w;
        const float k1 = 1.f / endCS.w;
        const vec3 q0 = startVS.xyz;// * k0;
        const vec3 q1 = endVS.xyz;// * k1;
        vec2 p0 = startCS.xy * k0 * 0.5 + 0.5f;
        vec2 p1 = endCS.xy * k1 * 0.5 + 0.5f;

        int stepCount = maxStepCount;

        float w0 = 0.f;
        float w1 = 0.f;
        bool hit = false;
        for (int i = 0; i < stepCount; i++)
        {
            w1 = w0;
            w0 += 1.f / float(stepCount);

            vec4 q = scene.proj * vec4(mix(q0, q1, w0), 1.f);
            vec2 rayUv = (q.xy / q.w) * 0.5 + 0.5;
            float k = q.w;

            if (rayUv.x < 0.f || rayUv.y < 0.f || rayUv.x > 1.f || rayUv.y > 1.f)
            {
                break;
            }

            const float sampleDepth = texture(textures[constants.depthMapIndex], rayUv).r;
            const float linearSampleDepth = linearizeDepth(sampleDepth, 0.1, 100.f);
            const float linearRayDepth = linearizeDepth(q.z / k, 0.1, 100.f);

            //const float depthDiff = linearRayDepth - linearSampleDepth;
            if (linearRayDepth > linearSampleDepth + thickness)
            //if (linearRayDepth > linearSampleDepth)
            {
                hit = true;
                break;
            }
        }

        // Binary search
        if (hit)
        {
            for (int i = 0; i < 8; i++)
            {
                float w = (w0 + w1) * 0.5;

                vec4 q = scene.proj * vec4(mix(q0, q1, w0), 1.f);
                vec2 rayUv = (q.xy / q.w) * 0.5 + 0.5;
                float k = q.w;

                const float sampleDepth = texture(textures[constants.depthMapIndex], rayUv).r;
                const float linearSampleDepth = linearizeDepth(sampleDepth, 0.1, 100.f);
                const float linearRayDepth = linearizeDepth(q.z, 0.1, 100.f);

                const float depthDiff = linearRayDepth - linearSampleDepth;
                if (linearRayDepth > linearSampleDepth)
                {
                    w1 = w;
                }
                else
                {
                    w0 = w;
                }
            }
        }

        outReflection = vec4(vec3(0.f), 1.f);
        if (hit)
        {
            vec4 q = scene.proj * vec4(mix(q0, q1, w1), 1.f);
            vec2 reflectedUv = (q.xy / q.w) * 0.5 + 0.5;

            vec3 worldPos = inverse(mat3(scene.view)) * mix(q0, q1, w1);
            float distFactor = clamp(0.f, 1.f, length(worldPos - startWS.xyz) / maxDist);

            vec3 L = reflectionDir;
            vec3 H = normalize(cameraDir + L);
            float attenuation = 1.f;

            vec3 f00 = mix(f0, vec3(1.f), metallicRoughness.r);

            // cook-torrance brdf
            float NDF = trowbridgeReitzGgx(n, H, metallicRoughness.y);
            float G   = smithGeometry(n, cameraDir, L, metallicRoughness.y);
            vec3 F    = fresnelSchlick(max(dot(H, cameraDir), 0.0), f00);

            vec3 kS = F;
            vec3 kD = vec3(1.0) - kS;
            kD *= 1.0 - metallicRoughness.x;

            vec3 numerator    = NDF * G * F;
            float denominator = 4.0 * max(dot(n, cameraDir), 0.0) * max(dot(n, L), 0.0) + 0.0001;
            vec3 specular     = numerator / denominator;

            // add to outgoing radiance Lo
            float NdotL = max(dot(n, L), 0.0);
            float reflectionIntensity = clamp(0.f, 1.f, (specular * NdotL).r);

            float visibility = 1.f
                // Alignment with the camera
                * clamp(0.f, 1.f, -dot(cameraDir, reflectionDir) + 0.975f)
                // Fade depending on the distance to the reflection
                * (exp(-distFactor * 2.f) * (1.f - distFactor))
                // Bounds check
                * (reflectedUv.x < 0.002f || reflectedUv.x > 0.998f ? 0.f : 1.f)
                * (reflectedUv.y < 0.002f || reflectedUv.y > 0.998f ? 0.f : 1.f)
                ;

            outReflection = vec4(reflectedUv, clamp(0.f, 1.f, visibility * reflectionIntensity), 1.f);
        }
    }

    //outColor = vec4(color * heatmapGradient(float(lightCount) / 32.f), 1.f);
    outColor = vec4(color, 1.f);

    //outColor = vec4(texture(textures[int(textureIndices.w)], uv).rgb, 1.f);
}
