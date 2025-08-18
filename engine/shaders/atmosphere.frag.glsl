#version 460

layout(push_constant) uniform Constants
{
    // TODO: convert to specialisation constant
    vec4 depth;
    vec4 sunDir;
} constants;

const float Re = 6360000; // Earth radius, in m
const float Ra = 6420000; // Atmosphere radius, in m

layout (location = 0) in vec2 uv;
layout (location = 0) out vec4 color;

#include "scene.glsl"
#include "utils.glsl"
#include "consts.glsl"

float nishitaHenyeyGreensteinPhaseFn(float cosAngle, float g)
{
    float g2 = g*g;
    return ((3.f * (1.f - g2)) / (2 * (2 + g2))) *
           ((1 + cosAngle*cosAngle) / pow(1 + g2 - 2 * g * cosAngle, 3.f/2.f));
}

float rayleighPhaseFn(float cosAngle)
{
    return nishitaHenyeyGreensteinPhaseFn(cosAngle, 0.f);

    //float cosAngle = cos(angle);
    //return (3.f / (16.f * PI)) * (1.f + cosAngle*cosAngle);
}

float miePhaseFn(float cosAngle)
{
    // TODO: pass as param
    return nishitaHenyeyGreensteinPhaseFn(cosAngle, -0.76f);
}

#define ITERATIONS 16
#define OD_ITERATIONS 16
#define DENSITY_FALLOFF 10.f

float atmosphereDensity(vec3 point, vec3 earthCenter, float earthRadius, float atmosphereRadius)
{
    float height = length(point - earthCenter) - earthRadius;
    float mappedHeight = height / (atmosphereRadius - earthRadius); // [0; 1]

    // TODO: add controllable param here
    return exp(-mappedHeight * DENSITY_FALLOFF) * (1.f - mappedHeight);
}

float opticalDepthN(vec3 point, vec3 rayDir, float rayLen, vec3 earthCenter, float earthRadius, float atmosphereRadius)
{
    float stepLength = rayLen / float(OD_ITERATIONS);
    float step = 1.f / float(OD_ITERATIONS);
    float opticalDepth = 0.f;

    vec3 samplePoint = point;

    for (int i = 0; i < OD_ITERATIONS; i++)
    {
        float density = atmosphereDensity(samplePoint, earthCenter, earthRadius, atmosphereRadius);

        opticalDepth += density * step;
        samplePoint += rayDir * stepLength;
    }

    return opticalDepth;
}

#define SCATTERING_STR 0.4f

void old_main()
{
    vec3 fragmentWS = deproject(vec3(uv * 2.f - 1.f, 1.f), inverse(scene.proj * scene.view));
    vec3 rayDir = normalize(fragmentWS - scene.cameraPos.xyz);

    const vec3 scatteringCoeffs = vec3(
        pow(400.f / 700.f, 4) * SCATTERING_STR,
        pow(400.f / 530.f, 4) * SCATTERING_STR,
        pow(400.f / 440.f, 4) * SCATTERING_STR
    );

    // Let's assume we're a little bit above the center of the planet
    //vec3 up = normalize((scene.view * vec4(0.f, 1.f, 0.f, 0.f)).xyz);
    vec3 up = vec3(0.f, 1.f, 0.f);
    vec3 planetCenter = scene.cameraPos.xyz - (Re - 100.f) * up;

    Hit hit = raySphereIntersection(scene.cameraPos.xyz, rayDir, planetCenter, Ra);
    if (!hit.hit)
    {
        color = vec4(1.f, 0.f, 1.f, 1.f);
        return;
    }
    // We know we hit the atmosphere

    //float inScatteredLight = 0.f;
    vec3 inScatteredLight = vec3(0.f);

    //float stepLength = 1.f;
    float stepLength = hit.t / float(ITERATIONS);
    float step = 1.f / float(ITERATIONS);
    vec3 samplePoint = scene.cameraPos.xyz;
    for (int i = 0; i < ITERATIONS; i++)
    {
        float sampleToAtmosphereLen = raySphereIntersection(samplePoint, constants.sunDir.xyz, planetCenter, Ra).t;

        //float sampleToAtmosphereOpticalDepth = opticalDepthN(samplePoint, vec3(0.f, 1.f, 0.f), sampleToAtmosphereLen, planetCenter, Re, Ra);
        float sampleToAtmosphereOpticalDepth = opticalDepthN(samplePoint, constants.sunDir.xyz, sampleToAtmosphereLen, planetCenter, Re, Ra);
        float sampleToCameraOpticalDepth = opticalDepthN(samplePoint, -rayDir, stepLength * float(i), planetCenter, Re, Ra);
        vec3 transmittance = exp(-(sampleToAtmosphereOpticalDepth + sampleToCameraOpticalDepth) * scatteringCoeffs);

        float singleTransmittance = exp(-(sampleToAtmosphereOpticalDepth));

        float density = atmosphereDensity(samplePoint, planetCenter, Re, Ra);

        samplePoint += rayDir * stepLength;
        inScatteredLight += density * transmittance * step * scatteringCoeffs;
        //inScatteredLight += density * singleTransmittance * step * vec3(1.f, 1.f, 1.f);
    }

    color = vec4(inScatteredLight, 1.f);
}






float rayleigh(float mu)
{
    return 3.f / (16.f * PI) * (1.f + mu*mu);
}

float mie(float mu, float g)
{
    float g2 = g * g;
    return 3.f / (8.f * PI) * ((1.f - g2) * (1.f + mu*mu)) / ((2.f + g2) * pow(1.f + g2 - 2.f * g * mu, 1.5f));
}

float opticalDepth(vec3 point, vec3 earthCenter, float earthRadius, float h)
{
    float height = length(point - earthCenter) - earthRadius;
    return exp(-height / h);
}

void main()
{
    vec3 fragmentWS = deproject(vec3(uv * 2.f - 1.f, 1.f), inverse(scene.proj * scene.view));
    vec3 rayDir = normalize(fragmentWS - scene.cameraPos.xyz);

    const float Hr = 7994.f;
    const float Hm = 1200.f;
    const vec3 betaR = vec3(3.8e-6, 13.5e-6, 33.1e-6);
    const vec3 betaM = vec3(21e-6);

    // Let's assume we're a little bit above the center of the planet
    vec3 up = vec3(0.f, 1.f, 0.f);
    vec3 planetCenter = scene.cameraPos.xyz - (Re - 100.f) * up;

    Hit hit = raySphereIntersection(scene.cameraPos.xyz, rayDir, planetCenter, Ra);
    if (!hit.hit)
    {
        color = vec4(1.f, 0.f, 1.f, 1.f);
        return;
    }
    // We know we hit the atmosphere

    //float inScatteredLight = 0.f;
    vec3 inScatteredLight = vec3(0.f);

    float mu = dot(rayDir, constants.sunDir.xyz);
    float rayleighPhase = rayleigh(mu);
    float miePhase = mie(mu, 0.76);

    vec3 sumR = vec3(0.f);
    vec3 sumM = vec3(0.f);

    float rayleighOpticalDepth = 0.f;
    float mieOpticalDepth = 0.f;

    //float stepLength = 1.f;
    float stepLength = hit.t / float(ITERATIONS);
    float step = 1.f / float(ITERATIONS);
    vec3 samplePoint = scene.cameraPos.xyz;
    for (int i = 0; i < ITERATIONS; i++)
    {
        float sampleToAtmosphereLen = raySphereIntersection(samplePoint, constants.sunDir.xyz, planetCenter, Ra).t;

        float sampleRayleighOpticalDepth = opticalDepth(samplePoint, planetCenter, Re, Hr) * stepLength;
        float sampleMieOpticalDepth = opticalDepth(samplePoint, planetCenter, Re, Hm) * stepLength;

        rayleighOpticalDepth += sampleRayleighOpticalDepth;
        mieOpticalDepth += sampleMieOpticalDepth;

#define INNER_ITERATIONS 16.f
        float innerRayleighOpticalDepth = 0.f;
        float innerMieOpticalDepth = 0.f;
        float innerStepLength = sampleToAtmosphereLen / float(INNER_ITERATIONS);
        float innerStep = 1.f / float(INNER_ITERATIONS);
        vec3 innerSamplePoint = samplePoint;
        for (int j = 0; j < INNER_ITERATIONS; j++)
        {
            innerRayleighOpticalDepth += opticalDepth(innerSamplePoint, planetCenter, Re, Hr) * innerStepLength;
            innerMieOpticalDepth += opticalDepth(innerSamplePoint, planetCenter, Re, Hm) * innerStepLength;
            innerSamplePoint += constants.sunDir.xyz * innerStepLength;
        }
        vec3 tau = betaR * (rayleighOpticalDepth + innerRayleighOpticalDepth) + betaM * 1.1f * (mieOpticalDepth + innerMieOpticalDepth);
        vec3 attenuation = vec3(exp(-tau.x), exp(-tau.y), exp(-tau.z));
        sumR += attenuation * sampleRayleighOpticalDepth;
        sumM += attenuation * sampleMieOpticalDepth;

        samplePoint += rayDir * stepLength;
    }
    inScatteredLight = (sumR * betaR * rayleighPhase + sumM * betaM * miePhase) * 5.f;

    color = vec4(inScatteredLight, 1.f);
}
