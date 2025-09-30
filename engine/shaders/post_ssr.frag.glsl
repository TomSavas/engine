#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

#include "bindless.glsl"
#include "scene.glsl"

layout(push_constant) uniform Constants
{
    layout(offset=16) uint color;
    uint normal;
    uint positions;
    uint reflectionUvs;
    uint blurredReflectionUvs;

    // DEBUG
    uint mode;

    float reflectionIntensity;
    float blurIntensity;
} constants;

const uint REFLECTION_UVS = 0;
const uint MASKED_REFLECTION_UVS = 1;
const uint COLOR = 2;
const uint REFLECTIONS = 3;
const uint BLEND = 4;

void main()
{
    vec4 clearReflectedUv = texture(textures[constants.reflectionUvs], uv);
    vec4 reflectedUv = mix(clearReflectedUv,
        texture(textures[constants.blurredReflectionUvs], uv), constants.blurIntensity);
    reflectedUv.b = clearReflectedUv.b;
    reflectedUv.b = clamp(0.f, 1.f, constants.reflectionIntensity * reflectedUv.b);

    vec3 pos = texture(textures[constants.positions], uv).xyz;
    vec3 pointNormal = texture(textures[constants.normal], uv).xyz;
    vec3 cameraDir = normalize(scene.cameraPos.xyz - pos);
    const vec3 toFrag = -cameraDir;
    const vec3 reflectionDir = normalize(reflect(toFrag, pointNormal));

    vec3 reflectionNormal = texture(textures[constants.normal], reflectedUv.xy).xyz;
    if (dot(reflectionDir, pointNormal) > 0.95)
    {
        reflectedUv.b = 0.f;
    }

    if (constants.mode == REFLECTION_UVS)
    {
        outColor = vec4(reflectedUv.rg, 0.f, 1.f);
    }
    else if (constants.mode == MASKED_REFLECTION_UVS)
    {
        outColor = vec4(mix(vec2(0.f), reflectedUv.rg, reflectedUv.b), 0.f, 1.f);
    }
    else if (constants.mode == COLOR)
    {
        outColor = vec4(texture(textures[constants.normal], uv).rgb, 1.f);

        outColor = vec4(vec3(dot(pointNormal, reflectionNormal)), 1.f);
    }
    else if (constants.mode == REFLECTIONS)
    {
        outColor = vec4(0.f, 0.f, 0.f, 1.f);
        if (reflectedUv.b != 0.f)
        {
            outColor = vec4(texture(textures[constants.color], reflectedUv.rg).rgb, 1.f);
        }
    }
    else if (constants.mode == BLEND)
    {
        vec4 reflectedColor = vec4(texture(textures[constants.color], reflectedUv.rg).rgb, 1.f);
        vec4 color = vec4(texture(textures[constants.color], uv).rgb, 1.f);

        outColor = mix(color, reflectedColor, reflectedUv.b);
        outColor = vec4(color.rgb + reflectedColor.rgb * reflectedUv.b, color.a);
    }
    else
    {
        outColor = vec4(texture(textures[ERROR_BINDLESS], uv).rgb, 1.f);
    }
}
