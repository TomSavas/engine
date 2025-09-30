#version 460
#extension GL_EXT_nonuniform_qualifier : require

#include "bindless.glsl"

layout (location = 0) in vec2 uv;

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform Constants
{
    // xy = resolution reciprocal
    // z = positionOffsetMultiplier
    // w = colorMultiplier
    layout(offset=16) vec4 params;
    int inputTexture;
};

void main()
{
	// Dual Kawase downsample: sample center + 4 diagonal corners
    const vec2 resolutionReciprocal = params.xy;
    const float positionOffsetMultiplier = params.z;
    const float colorMultiplier = params.w;

	vec2 halfpixel = resolutionReciprocal * 0.5;
	vec2 o = halfpixel * positionOffsetMultiplier;

	// Sample center with 4x weight
	vec4 color = texture(textures[inputTexture], uv) * 4.0;

	// Sample 4 diagonal corners with 1x weight each
	color += texture(textures[inputTexture], uv + vec2(-o.x, -o.y)); // bottom-left
	color += texture(textures[inputTexture], uv + vec2( o.x, -o.y)); // bottom-right
	color += texture(textures[inputTexture], uv + vec2(-o.x,  o.y)); // top-left
	color += texture(textures[inputTexture], uv + vec2( o.x,  o.y)); // top-right

	// Apply bloom strength and normalize by total weight (8)
	outColor = (color / 8.0) * colorMultiplier;
}