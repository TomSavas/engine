#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
// TODO: this should be passed in from outside
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

uint floatToSortableUint(float f) {
    uint i = floatBitsToUint(f);
    return (i & 0x80000000u) != 0u ? ~i : (i | 0x80000000u);
}

float sortableUintToFloat(uint u) {
    u = (u & 0x80000000u) != 0u ? (u & ~0x80000000u) : ~u;
    return uintBitsToFloat(u);
}

void main()
{
    uint tileId = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint threadsPerWorkGroup = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

    if (gl_LocalInvocationIndex == 0)
    {
        //minDepth = 1;
        minDepth = 0xffffffff;
        //maxDepth = -10000000;
        maxDepth = 0;
    }
    barrier();

    vec2 tileSizeInPixels = vec2(1920.f, 1080.f) / gl_NumWorkGroups.xy;
    vec2 threadSizeInPixels = tileSizeInPixels / gl_WorkGroupSize.xy;
    vec2 topLeftTilePixel = gl_WorkGroupID.xy * tileSizeInPixels;
    vec2 threadUv = (topLeftTilePixel + (gl_LocalInvocationID.xy * threadSizeInPixels)) / vec2(1920.f, 1080.f);

    float depth = texture(textures[constants.depthIndex], threadUv).r;
    atomicMin(minDepth, floatBitsToUint(depth));
    atomicMax(maxDepth, floatBitsToUint(depth));
    //atomicMin(minDepth, floatToSortableUint(depth));
    //atomicMax(maxDepth, floatToSortableUint(depth));

    barrier();

    float linearizedMin = uintBitsToFloat(minDepth);
    float linearizedMax = uintBitsToFloat(maxDepth);
    //float linearizedMin = sortableUintToFloat(minDepth);
    //float linearizedMax = sortableUintToFloat(maxDepth);

    barrier();

    if (gl_LocalInvocationIndex == 0)
    {
        localLightIdCount = 0;

        vec2 res = vec2(1920.f, 1080.f);
        vec2 topLeft = topLeftTilePixel / res;
        vec2 topRight = (topLeftTilePixel + vec2(tileSizeInPixels.x, 0)) / res;
        vec2 bottomLeft = (topLeftTilePixel + vec2(0, tileSizeInPixels.y)) / res;
        vec2 bottomRight = (topLeftTilePixel + vec2(tileSizeInPixels.x, tileSizeInPixels.y)) / res;

        mat4 p = scene.proj;
        mat4 invProj = inverse(p);
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

    mat4 p = scene.proj;
    mat4 invProj = inverse(p);
    float minDepthVS = -ndcToView(vec3(0, 0, linearizedMin * 2.f - 1.f), invProj).z;
    float maxDepthVS = -ndcToView(vec3(0, 0, linearizedMax), invProj).z;

    uint lightsToCheckPerThread = (constants.lights.pointLightCount + threadsPerWorkGroup - 1) / threadsPerWorkGroup;
    uint firstLight = lightsToCheckPerThread * gl_LocalInvocationIndex;
    uint lastLight = min(constants.lights.pointLightCount, firstLight + lightsToCheckPerThread);

    //uint lightsToCheckPerThread = 1;
    //for (int i = 0; i < lastLight; i++)
    for (uint i = firstLight; i < lastLight; i++)
    {
        //uint lightId = gl_LocalInvocationIndex * lightsToCheckPerThread + i;
        //uint lightId = 0;
        uint lightId = i;
        if (lightId >= MAX_POINT_LIGHTS)
            break;

        PointLight light = constants.lights.pointLights[lightId];
        vec4 lightPosVS = scene.view * vec4(light.pos.xyz, 1.f);

        // Near-far testing first
        //bool overlapsFrustum = true;
        bool overlapsFrustum = ((-lightPosVS.z - light.range.x) < maxDepthVS);
        // FIXME: for some reason using minDepthVS doesn't work here :(
        //overlapsFrustum = overlapsFrustum && ((-lightPosVS.z + light.range.x) > maxDepthVS);
        overlapsFrustum = overlapsFrustum && ((-lightPosVS.z + light.range.x) > minDepthVS);

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





















































/*
void main()
{
    uint tileId = gl_WorkGroupID.y * gl_NumWorkGroups.x + gl_WorkGroupID.x;
    uint threadsPerWorkGroup = gl_WorkGroupSize.x * gl_WorkGroupSize.y;

    if (gl_LocalInvocationIndex == 0)
    {
        minDepth = 10000000;
        maxDepth = -10000000;
    }
    barrier();

    vec2 tileSizeInPixels = vec2(1920.f, 1080.f) / gl_NumWorkGroups.xy;
    vec2 threadSizeInPixels = tileSizeInPixels / gl_WorkGroupSize.xy;
    vec2 topLeftTilePixel = gl_WorkGroupID.xy * tileSizeInPixels;
    vec2 uv = topLeftTilePixel + (gl_LocalInvocationID.xy) * threadSizeInPixels;
    uv = uv / vec2(1920.f, 1080.f);

    vec2 nextUv = topLeftTilePixel + tileSizeInPixels;
    nextUv = nextUv / vec2(1920.f, 1080.f);
    //uv.y = 1.f - uv.y;

    float depth = texture(textures[constants.depthIndex], uv).r;
    atomicMin(minDepth, int(depth * 100000.f));
    atomicMax(maxDepth, int(depth * 100000.f));

    barrier();

    if (gl_LocalInvocationIndex == 0)
    {
        localLightIdCount = 0;

        vec2 center = gl_NumWorkGroups.xy / 2.f;
        vec2 offset = center - gl_WorkGroupID.xy;

        mat4 vp = scene.proj;

        vec4 column0 = vec4(-vp[0][0] * center.x, vp[0][1], offset.x, vp[0][3]);
        vec4 column1 = vec4(vp[1][0], vp[1][1] * center.y, offset.y, vp[1][3]);
        //vec4 column2 = vec4(vp[2][0], vp[2][1], 1.0f, vp[2][3]);
        vec4 column3 = vec4(vp[3][0], vp[3][1], -1.0f, vp[3][3]);

        frustumPlanes[0] = column3 + column0;
        frustumPlanes[1] = column3 - column0;
        frustumPlanes[2] = column3 - column1;
        frustumPlanes[3] = column3 + column1;
        float linearizedMin = linearizeDepthFromCameraParams(float(minDepth) / 100000.f);
        frustumPlanes[4] = vec4(0.f, 0.f, -1.f, -linearizedMin);
        //frustumPlanes[4] = vec4(0.f, 0.f, -1.f, -float(minDepth) / 100000.f);
        //frustumPlanes[4] = vec4(0.f, 0.f, -1.f, -0.1f);
        // I have very high depth values in my scene, so depth precision errors need to be accounted for
        // TODO: rebuild test scene with smaller scales to avoid these depth precision issues
        const float depthPrecisionHack = 1.f;
        float linearizedMax = linearizeDepthFromCameraParams(float(maxDepth) / 100000.f) * depthPrecisionHack;
        frustumPlanes[5] = vec4(0.f, 0.f, 1.f, linearizedMax);
        //frustumPlanes[5] = vec4(0.f, 0.f, 1.f, float(maxDepth) / 100000.f);
        //frustumPlanes[5] = vec4(0.f, 0.f, 1.f, 5000.f);
        for (int i = 0; i < 6; i++)
        {
            frustumPlanes[i] /= length(frustumPlanes[i].xyz);
        }
    }
    barrier();

    uint lightsToCheckPerThread = (constants.lights.pointLightCount + threadsPerWorkGroup - 1) / threadsPerWorkGroup;
    //uint lightsToCheckPerThread = 1;
    for (int i = 0; i < lightsToCheckPerThread; i++)
    {
        uint lightId = gl_LocalInvocationIndex * lightsToCheckPerThread + i;
        if (lightId >= MAX_POINT_LIGHTS)
            break;
        PointLight light = constants.lights.pointLights[lightId];

        bool overlapsFrustum = true;
        vec4 lightPosInViewSpace = scene.view * vec4(light.pos.xyz, 1.f);
        //lightPosInViewSpace /= lightPosInViewSpace.w;
        for (int j = 0; j < 6 && overlapsFrustum; j++)
        {
            float distance = dot(frustumPlanes[j], lightPosInViewSpace); // Distance of the point from the plane
            // https://gamedev.stackexchange.com/questions/79172/checking-if-a-vector-is-contained-inside-a-viewing-frustum
            overlapsFrustum = -light.range.x <= distance;
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
*/
