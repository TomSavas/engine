#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
layout (local_size_x = 32, local_size_y = 32) in;

#include "lights.glsl"
#include "utils.glsl"

layout(set = 0, binding = 0) uniform SceneUniforms
{
    vec4 cameraPos;
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 0) uniform sampler2D textures[];

layout(push_constant) uniform Constants
{
    uint depthIndex;
    Lights lights;
    LightIds ids;
    LightTileData lightTiles;
    // TEMP:
    LightIdCount count;
} constants;

#define MAX_LIGHTS_PER_TILE (64)
#define MAX_POINT_LIGHTS (16384)

shared uint localLightIdCount;
shared uint localLightIds[MAX_LIGHTS_PER_TILE];
shared Plane frustumPlanes[4];
shared uint minDepth;
shared uint maxDepth;

void main()
{
    uint tileId = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint threadsPerWorkGroup = gl_WorkGroupSize.x * gl_WorkGroupSize.y;
    mat4 invProj = inverse(scene.proj);

    if (gl_LocalInvocationIndex == 0)
    {
        minDepth = 0xffffffff; // Or floatBitsToUint(1.f)
        maxDepth = 0;
    }
    barrier();

    vec2 tileSizeInUv = vec2(1.f, 1.f) / gl_NumWorkGroups.xy;
    vec2 threadSizeInUv = tileSizeInUv / gl_WorkGroupSize.xy;
    vec2 tileTopLeftUv = gl_WorkGroupID.xy * tileSizeInUv;
    vec2 threadUv = (tileTopLeftUv + (gl_LocalInvocationID.xy * threadSizeInUv));

    float depth = texture(textures[constants.depthIndex], threadUv).r;
    atomicMin(minDepth, floatBitsToUint(depth));
    atomicMax(maxDepth, floatBitsToUint(depth));

    barrier();

    if (gl_LocalInvocationIndex == 0)
    {
        localLightIdCount = 0;

        vec2 topLeft = tileTopLeftUv;
        vec2 topRight = (tileTopLeftUv + vec2(tileSizeInUv.x, 0));
        vec2 bottomLeft = (tileTopLeftUv + vec2(0, tileSizeInUv.y));
        vec2 bottomRight = (tileTopLeftUv + tileSizeInUv);

        vec3 frustumCorners[8] = {
            ndcToView(vec3(topLeft * 2.f - 1.f, -1.f), invProj),     // 0
            ndcToView(vec3(topRight * 2.f - 1.f, -1.f), invProj),    // 1
            ndcToView(vec3(bottomLeft * 2.f - 1.f, -1.f), invProj),  // 2
            ndcToView(vec3(bottomRight * 2.f - 1.f, -1.f), invProj), // 3
            ndcToView(vec3(topLeft * 2.f - 1.f, 1.f), invProj),     // 4
            ndcToView(vec3(topRight * 2.f - 1.f, 1.f), invProj),    // 5
            ndcToView(vec3(bottomLeft * 2.f - 1.f, 1.f), invProj),  // 6
            ndcToView(vec3(bottomRight * 2.f - 1.f, 1.f), invProj), // 7
        };

        frustumPlanes[0] = planeFromPoints(frustumCorners[0], frustumCorners[2], frustumCorners[4]); // left
        frustumPlanes[1] = planeFromPoints(frustumCorners[3], frustumCorners[1], frustumCorners[5]); // right
        frustumPlanes[2] = planeFromPoints(frustumCorners[1], frustumCorners[0], frustumCorners[4]); // top
        frustumPlanes[3] = planeFromPoints(frustumCorners[2], frustumCorners[3], frustumCorners[6]); // bottom

        // wtf these are flipped in terms of normals
        //frustumPlanes[4] = planeFromPoints(frustumCorners[1], frustumCorners[0], frustumCorners[3]); // near
        //frustumPlanes[5] = planeFromPoints(frustumCorners[4], frustumCorners[5], frustumCorners[6]); // far
    }
    barrier();

    float minDepthVS = -ndcToView(vec3(0, 0, uintBitsToFloat(minDepth)), invProj).z;
    float maxDepthVS = -ndcToView(vec3(0, 0, uintBitsToFloat(maxDepth)), invProj).z;

    uint lightsToCheckPerThread = (constants.lights.pointLightCount + threadsPerWorkGroup - 1) / threadsPerWorkGroup;
    uint firstLight = lightsToCheckPerThread * gl_LocalInvocationIndex;
    uint lastLight = min(constants.lights.pointLightCount, firstLight + lightsToCheckPerThread);
    for (uint i = firstLight; i < lastLight; i++)
    {
        uint lightId = i;
        if (lightId >= MAX_POINT_LIGHTS)
            break;

        PointLight light = constants.lights.pointLights[lightId];
        vec4 lightPosVS = scene.view * vec4(light.pos.xyz, 1.f);

        // Near-far testing first
        bool overlapsFrustum = ((-lightPosVS.z - light.range.x) < maxDepthVS) &&
                               ((-lightPosVS.z + light.range.x) > minDepthVS);
        for (int j = 0; j < 4 && overlapsFrustum; j++)
        {
            float lightToPlane = dot(frustumPlanes[j].n, lightPosVS.xyz) - frustumPlanes[j].d;
            overlapsFrustum = overlapsFrustum && (0 <= lightToPlane + light.range.x);
        }

        if (overlapsFrustum)
        {
            localLightIds[atomicAdd(localLightIdCount, 1)] = lightId;
        }
    }
    barrier();

    // TODO: maybe allow all threads to write some values?
    if (gl_LocalInvocationIndex == 0)
    {
        uint lightIdStart = atomicAdd(constants.count.lightIdCount, localLightIdCount);
        constants.lightTiles.tiles[tileId].count = localLightIdCount;
        constants.lightTiles.tiles[tileId].offset = lightIdStart;

        for (int i = 0; i < localLightIdCount; i++)
        {
            constants.ids.ids[lightIdStart + i] = localLightIds[i];
        }
    }
}