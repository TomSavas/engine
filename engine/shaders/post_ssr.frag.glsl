#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

#include "bindless.glsl"

layout(push_constant) uniform Constants
{
    layout(offset=16) uint color;
    uint normal;
    uint reflectionUvs;

    // DEBUG
    uint mode;
} constants;

const uint REFLECTION_UVS = 0;
const uint COLOR = 1;
const uint REFLECTIONS = 2;
const uint BLEND = 3;

void main()
{
    vec4 reflectedUv = texture(textures[constants.reflectionUvs], uv);
    if (constants.mode == REFLECTION_UVS)
    {
        //outColor = reflectedUv;
        outColor = vec4(reflectedUv.rg, 0.f, 1.f);
    }
    else if (constants.mode == COLOR)
    {
        //outColor = vec4(texture(textures[constants.color], uv).rgb, 1.f);
        outColor = vec4(texture(textures[constants.normal], uv).rgb, 1.f);
    }
    else if (constants.mode == REFLECTIONS)
    {
        outColor = vec4(0.f, 0.f, 0.f, 1.f);
        //if (reflectedUv.r != 0.f && reflectedUv.g != 0.f)
        if (reflectedUv.b != 0.f)
        {
            //outColor = vec4(texture(textures[constants.color], uv).rgb, 1.f);
            outColor = vec4(texture(textures[constants.color], reflectedUv.rg).rgb, 1.f);
        }
    }
    else if (constants.mode == BLEND)
    {
        //outColor = vec4(texture(textures[constants.color], reflectedUv.rg).rgb, 1.f);
        //if (reflectedUv.r == 0.f && reflectedUv.g == 0)
        //if (reflectedUv.b == 0)
        //{
        //    outColor = vec4(texture(textures[constants.color], uv).rgb, 1.f);
        //}

        vec4 reflectedColor = vec4(texture(textures[constants.color], reflectedUv.rg).rgb, 1.f);
        vec4 color = vec4(texture(textures[constants.color], uv).rgb, 1.f);

        outColor = mix(color, reflectedColor, reflectedUv.b);
        //outColor = vec4(vec3(reflectedUv.b), 1.f);
    }
    else
    {
        outColor = vec4(texture(textures[ERROR_BINDLESS], uv).rgb, 1.f);
    }
}
