#version 460

layout(push_constant) uniform Constants
{
    // TODO: convert to specialisation constant
    vec4 depth;
    vec4 sunDirAndIntensity;
    vec4 scatteringCoeffs;
};

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

#include "scene.glsl"
#include "utils.glsl"
#include "consts.glsl"

#define SAMPLE_ITERATIONS 16
#define OPTICAL_DEPTH_ITERATIONS 8

const float eR = 6360e3; // Earth radius, in m
const float aR = 6420e3; // Atmosphere radius, in m

const float rHeightScale = 7994.f;
const float mHeightScale = 1200.f;

const float mieAsymmetryParam = 0.76f;
const float mieExtinctionCoeff = 1.1f;

// Default values used by Bruneton and Neyret
// const vec3 rScatteringCoeff = vec3(5.8e-6, 13.5e-6, 33.1e-6); // Usually marked with betaR
// const vec3 mScatteringCoeff = vec3(21e-6); // Usually marked with betaM

float rayleighPhase(float mu)
{
    return 3.f / (16.f * PI) * (1.f + mu*mu);
}

float miePhase(float mu, float g)
{
    const float g2 = g * g;
    return 3.f / (8.f * PI) * ((1.f - g2) * (1.f + mu*mu)) / ((2.f + g2) * pow(1.f + g2 - 2.f * g * mu, 1.5f));
}

float opticalDepth(vec3 point, vec3 earthCenter, float earthRadius, float h)
{
    const float height = length(point - earthCenter) - earthRadius;
    return exp(-height / h);
}

vec2 opticalDepthTowards(vec3 samplePoint, vec3 dir, float rayLength, vec3 planetCenter)
{
    const float stepLength = rayLength / float(OPTICAL_DEPTH_ITERATIONS);

    float rOpticalDepth = 0.f;
    float mOpticalDepth = 0.f;
    for (int j = 0; j < OPTICAL_DEPTH_ITERATIONS; j++)
    {
        rOpticalDepth += opticalDepth(samplePoint, planetCenter, eR, rHeightScale) * stepLength;
        mOpticalDepth += opticalDepth(samplePoint, planetCenter, eR, mHeightScale) * stepLength;
        samplePoint += dir * stepLength;
    }

    return vec2(rOpticalDepth, mOpticalDepth);
}

void main()
{
    const vec3 rScatteringCoeff = scatteringCoeffs.xyz;
    const vec3 mScatteringCoeff = vec3(scatteringCoeffs.w);

    const vec3 fragmentWS = deproject(vec3(uv * 2.f - 1.f, 1.f), inverse(scene.proj * scene.view));
    const vec3 rayDir = normalize(fragmentWS - scene.cameraPos.xyz);

    // Let's assume we're a little bit above the center of the planet
    const vec3 up = vec3(0.f, 1.f, 0.f);
    const vec3 planetCenter = -eR * up;

    Hit hit = raySphereIntersection(scene.cameraPos.xyz, rayDir, planetCenter, aR);
    if (!hit.hit)
    {
        color = vec4(1.f, 0.f, 1.f, 1.f);
        return;
    }

    vec3 rSum = vec3(0.f);
    vec3 mSum = vec3(0.f);

    float rOpticalDepth = 0.f;
    float mOpticalDepth = 0.f;

    const float stepLength = hit.t / float(SAMPLE_ITERATIONS);
    vec3 samplePoint = scene.cameraPos.xyz;
    for (int i = 0; i < SAMPLE_ITERATIONS; i++)
    {
        const float rOpticalDepthAtSample = opticalDepth(samplePoint, planetCenter, eR, rHeightScale) * stepLength;
        const float mOpticalDepthAtSample = opticalDepth(samplePoint, planetCenter, eR, mHeightScale) * stepLength;
        rOpticalDepth += rOpticalDepthAtSample;
        mOpticalDepth += mOpticalDepthAtSample;

        const float sampleToAtmosphereLen = raySphereIntersection(samplePoint, sunDirAndIntensity.xyz, planetCenter, aR).t;
        const vec2 opticalDepthTowardsSun = opticalDepthTowards(samplePoint, sunDirAndIntensity.xyz, sampleToAtmosphereLen, planetCenter);
        const float rOpticalDepthTowardsSun = opticalDepthTowardsSun.x;
        const float mOpticalDepthTowardsSun = opticalDepthTowardsSun.y;

        const vec3 opticalDepth = rScatteringCoeff * (rOpticalDepth + rOpticalDepthTowardsSun) +
                                  mScatteringCoeff * (mOpticalDepth + mOpticalDepthTowardsSun) * mieExtinctionCoeff;
        const vec3 attenuation = exp(-opticalDepth);
        rSum += attenuation * rOpticalDepthAtSample;
        mSum += attenuation * mOpticalDepthAtSample;

        samplePoint += rayDir * stepLength;
    }

    const float mu = dot(rayDir, sunDirAndIntensity.xyz);
    const float rPhase = rayleighPhase(mu);
    const float mPhase = miePhase(mu, mieAsymmetryParam);

    const vec3 c = (rSum * rPhase * rScatteringCoeff + mSum * mPhase * mScatteringCoeff) * sunDirAndIntensity.w;
    color = vec4(c, 1.f);
}