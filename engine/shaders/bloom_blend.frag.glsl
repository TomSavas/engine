#version 460
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shading_language_include : require

#include "bindless.glsl"

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform Constants
{
    layout(offset=16) vec4 strength;
    uint blurredInput;
    uint clearInput;
};

void main()
{
    outColor = mix(texture(textures[clearInput], uv), texture(textures[blurredInput], uv), strength.x);
}
