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
layout (location = 2) out vec4 reflection;

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

float getThicknessDiff(float diff, float linearSampleDepth, float thickness)
{
    return (diff - thickness) / linearSampleDepth;
}

void main()
{
    const vec4 textureIndices = constants.modelData.data[index].textures;
    //vec2 uv = parallaxOcclussionMapBinarySearch(vert_uv, int(textureIndices.z));
    vec2 uv = vert_uv;

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

	//vec3 n = normalize(tbn * (texNormal * vec3(2.f) - vec3(1.f)));
	vec3 n = tbn[2];
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

    // Selection highlight
    if (constants.modelData.data[index].selected.x > 0.f)
    {
        color = mix(color, vec3(1.f, 1.f, 1.f), cos(scene.time.x * 5.f) * 0.5f + 0.5f);
    }

    //// SSR reflection buffer generation
    //vec3 reflectionUV = vec3(-1.f, -1.f, -1.f);
    //{
    //    //const float resolution = 0.3;
    //    const float resolution = 1;
    //    const float thickness = 0.01;

    //    vec3 toFrag = -cameraDir;
    //    vec3 reflectionDir = normalize(reflect(toFrag, n));
    //    const float maxDist = 10.f;

    //    const mat4 mvp = scene.proj * scene.view;
    //    vec4 startWorld = vec4(pos, 1.f);
    //    vec4 start = mvp * startWorld;
    //    start /= start.w;
    //    vec2 startUv = start.xy * 0.5 + 0.5;

    //    vec4 endWorld = vec4(pos + reflectionDir * maxDist, 1.f);
    //    vec4 end = mvp * endWorld;
    //    end /= end.w;
    //    vec2 endUv = end.xy * 0.5 + 0.5;

    //    vec3 deltaPos = (endWorld - startWorld).xyz;

    //    vec2 deltaUv = endUv - startUv;
    //    float useX = abs(deltaUv.x) >= abs(deltaUv.y) ? 1.0 : 0.0;
    //    float largerDelta = mix(abs(deltaUv.y), abs(deltaUv.x), useX) * clamp(resolution, 0.0, 1.0);
    //    //vec2 incrementUv = deltaUv / max(largerDelta, 0.001f) / vec2(1920.f, 1080.f);

    //    // TODO: inject actual resolution
    //    //int stepCount = int(largerDelta * mix(1920.f, 1080.f, useX));
    //    int stepCount = 64;
    //    vec3 incrementPos = deltaPos / float(stepCount);
    //    vec2 incrementUv = deltaUv / float(stepCount);

    //    // Screen space ray marching
    //    vec2 currentUv = startUv;
    //    vec3 currentPos = startWorld.xyz;
    //    const mat4 invViewProj = inverse(mvp);

    //    vec3 preHitPos = currentPos;
    //    vec2 preHitUv = currentUv;

    //    bool hit = false;

    //    for (int i = 0; i < stepCount; i++)
    //    {
    //        currentPos += incrementPos;
    //        currentUv += incrementUv;

    //        if (currentUv.x < 0.f || currentUv.x > 1.f || currentUv.y < 0.f || currentUv.y > 1.f)
    //        {
    //            //hit = false;
    //            break;
    //        }

    //        //const float currentDepth = length(currentPos - scene.cameraPos.xyz);
    //        vec4 a = mvp * vec4(currentPos, 1.f);
    //        const float currentDepth = linearizeDepth(a.z / a.w, 0.1, 100.f);
    //        currentUv = (a.xy / a.w) * 0.5 + 0.5;

    //        const float depth = linearizeDepth(texture(textures[constants.depthMapIndex], currentUv).r, 0.1, 100.f);
    //        //const vec3 deprojectedPos = deproject(vec3(currentUv * 2.f - 1.f, depth), invViewProj);

    //        // really don't need to do this, we already have depth...
    //        //const float recalculatedDepth = length(deprojectedPos - scene.cameraPos.xyz);
    //        //const float recalculatedDepth = linearizeDepth(depth, 0.1f, 100.f);
    //        const float recalculatedDepth = depth;

    //        // Hit
    //        //if (currentDepth > (recalculatedDepth - thickness))
    //        if (currentDepth > recalculatedDepth && (currentDepth - recalculatedDepth) < thickness)
    //        {
    //            hit = true;
    //            break;
    //        }

    //        preHitPos = currentPos;
    //        preHitUv = currentUv;
    //    }

    //    vec3 midPos = (currentPos + preHitPos) / 2.f;
    //    vec2 midUv = (currentUv + preHitUv) / 2.f;
    //    for (int i = 0; i < 16; i++)
    //    {
    //        //const float currentDepth = length(midPos - scene.cameraPos.xyz);
    //        vec4 a = mvp * vec4(midPos, 1.f);
    //        const float currentDepth = linearizeDepth(a.z / a.w, 0.1, 100.f);

    //        const float depth = linearizeDepth(texture(textures[constants.depthMapIndex], currentUv).r, 0.1, 100.f);
    //        //const vec3 deprojectedPos = deproject(vec3(currentUv * 2.f - 1.f, depth), invViewProj);

    //        // really don't need to do this, we already have depth...
    //        //const float recalculatedDepth = length(deprojectedPos - scene.cameraPos.xyz);
    //        const float recalculatedDepth = depth;

    //        //if (currentDepth > (recalculatedDepth - thickness))
    //        //if (currentDepth > recalculatedDepth)
    //        if (currentDepth > recalculatedDepth && (currentDepth - recalculatedDepth) < thickness)
    //        {
    //            preHitPos = midPos;
    //            //preHitUv = midUv;
    //            a = mvp * vec4(midPos, 1.f);
    //            preHitUv = (a.xy / a.w) * 0.5 + 0.5;
    //        }
    //        else
    //        {
    //            currentPos = midPos;
    //            //currentUv = midUv;
    //            a = mvp * vec4(midPos, 1.f);
    //            currentUv = (a.xy / a.w) * 0.5 + 0.5;
    //        }
    //        midPos = (currentPos + preHitPos) / 2.f;
    //        midUv = (currentUv + preHitUv) / 2.f;

    //        //a = mvp * vec4(midPos, 1.f);
    //        //midUv = (a.xy / a.w) * 0.5 + 0.5;
    //    }

    //    //color = vec3(endUv, 0.f);
    //    reflection = vec4(vec3(0.f), 1.f);
    //    if (hit)
    //    {
    //        //float distPerc = length(currentPos - startWorld.xyz) / ((maxDist / 10.f) * (maxDist / 10.f));

    //        //color = mix(vec3(currentUv, 0.f), color, clamp(0.f, 1.f, distPerc));

    //        vec3 L = reflectionDir;
    //        vec3 H = normalize(cameraDir + L);
    //        float attenuation = 1.f;

    //        // cook-torrance brdf
    //        float NDF = trowbridgeReitzGgx(n, H, metallicRoughness.y);
    //        float G   = smithGeometry(n, cameraDir, L, metallicRoughness.y);
    //        vec3 F    = fresnelSchlick(max(dot(H, cameraDir), 0.0), f0);

    //        vec3 kS = F;
    //        vec3 kD = vec3(1.0) - kS;
    //        kD *= 1.0 - metallicRoughness.x;

    //        vec3 numerator    = NDF * G * F;
    //        float denominator = 4.0 * max(dot(n, cameraDir), 0.0) * max(dot(n, L), 0.0) + 0.0001;
    //        vec3 specular     = numerator / denominator;

    //        // add to outgoing radiance Lo
    //        float NdotL = max(dot(n, L), 0.0);
    //        float reflectionIntensity = (specular * NdotL).r;

    //        float distFactor = clamp(0.f, 1.f, length(midPos - startWorld.xyz) / (maxDist / 5.f));
    //        float visibility = 1.f
    //            // Alignment with the camera
    //            * max(0.f, -dot(cameraDir, reflectionDir))
    //            // Fade depending on the distance to the reflection
    //            //* (1.f - clamp(0.f, 1.f, length(midPos - startWorld.xyz) / maxDist))
    //            //* exp(-distFactor * 2.f) * (1.f - distFactor)
    //            // Bounds check
    //            * (midUv.x < 0.f || midUv.x > 1.f ? 0.f : 1.f)
    //            * (midUv.y < 0.f || midUv.y > 1.f ? 0.f : 1.f)
    //            ;

    //        //color = vec3(currentUv, 0.f);
    //        //reflection = vec4(currentUv, 0.f, 1.f);
    //        //reflection = vec4(midUv, visibility, 1.f);
    //        //reflection = vec4(midUv, reflectionIntensity * visibility, 1.f);
    //        //reflection = vec4(metallicRoughness, 0.f, 1.f);
    //        reflection = vec4(midUv, visibility, 1.f);
    //    }
    //}

    // SSR v2.0
    {
        const mat4 mvp = scene.proj * scene.view;
        const float maxDist = 10.f;
        const int stepCount = 64;
        const float thickness = 0.07;

        const vec3 toFrag = -cameraDir;
        const vec3 reflectionDir = normalize(reflect(toFrag, n));

        const vec4 startWS = vec4(pos, 1.f);
        const vec4 startCS = mvp * startWS;
        const vec4 endWS = vec4(startWS.xyz + reflectionDir * maxDist, 1.f);
        const vec4 endCS = mvp * endWS;

        const float k0 = 1.f / startCS.w;
        const float k1 = 1.f / endCS.w;
        const vec3 q0 = startCS.xyz * k0;
        const vec3 q1 = endCS.xyz * k1;
        const vec2 startUv = startCS.xy * k0 * 0.5 + 0.5f;
        vec2 endUv = endCS.xy * k1 * 0.5 + 0.5f;
        endUv += vec2((distanceSquared(startUv, endUv) < 0.0001) ? 0.01 : 0.0);

        float w0 = 0.f;
        float w1 = 0.f;
        bool hit = false;
        for (int i = 0; i < stepCount; i++)
        {
            w1 = w0;
            //w0 += 1.f / float(stepCount);
            w0 = float(i+1) / float(stepCount);

            vec3 q = mix(q0, q1, w0);
            vec2 rayUv = mix(startUv, endUv, w0);
            float k = mix(k0, k1, w0);

            const float sampleDepth = texture(textures[constants.depthMapIndex], rayUv).r;
            const float linearSampleDepth = linearizeDepth(sampleDepth, 0.1, 100.f);
            const float linearRayDepth = linearizeDepth(q.z, 0.1, 100.f);

            const float depthDiff = linearRayDepth - linearSampleDepth;
            const float thicknessDiff = getThicknessDiff(depthDiff, linearSampleDepth, thickness);
            //if (linearRayDepth > linearSampleDepth && (linearRayDepth - linearSampleDepth) < thickness)
            arRayDepth > linearSampleDepth && linearRayDepth > linearSampleDepth + thickness)
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

                vec3 q = mix(q0, q1, w);
                vec2 rayUv = mix(startUv, endUv, w0);
                float k = mix(k0, k1, w);

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

        reflection = vec4(vec3(0.f), 1.f);
        if (hit)
        {
            vec2 reflectedUv = mix(startUv, endUv, w1);

            float visibility = 1.f
                // Alignment with the camera
                * max(0.f, -dot(cameraDir, reflectionDir))
                // Fade depending on the distance to the reflection
                //* (1.f - clamp(0.f, 1.f, length(midPos - startWorld.xyz) / maxDist))
                //* exp(-distFactor * 2.f) * (1.f - distFactor)
                // Bounds check
                * (reflectedUv.x < 0.f || reflectedUv.x > 1.f ? 0.f : 1.f)
                * (reflectedUv.y < 0.f || reflectedUv.y > 1.f ? 0.f : 1.f)
                ;

            reflection = vec4(reflectedUv, visibility, 1.f);
        }
    }

    //outColor = vec4(color * heatmapGradient(float(lightCount) / 32.f), 1.f);
    outColor = vec4(color, 1.f);
}
