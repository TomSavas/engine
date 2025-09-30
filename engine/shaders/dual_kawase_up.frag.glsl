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
	// Dual Kawase upsample: sample 4 edge centers + 4 diagonal corners
    const vec2 resolutionReciprocal = params.xy;
    const float positionOffsetMultiplier = params.z;
    const float colorMultiplier = params.w;

	vec2 halfpixel = resolutionReciprocal * 0.5;
	vec2 o = halfpixel * positionOffsetMultiplier;

	vec4 color = vec4(0.0);

	// Sample 4 edge centers with 1x weight each
	color += texture(textures[inputTexture], uv + vec2(-o.x * 2.0, 0.0)); // left
	color += texture(textures[inputTexture], uv + vec2( o.x * 2.0, 0.0)); // right
	color += texture(textures[inputTexture], uv + vec2(0.0, -o.y * 2.0)); // bottom
	color += texture(textures[inputTexture], uv + vec2(0.0,  o.y * 2.0)); // top

	// Sample 4 diagonal corners with 2x weight each
	color += texture(textures[inputTexture], uv + vec2(-o.x,  o.y)) * 2.0; // top-left
	color += texture(textures[inputTexture], uv + vec2( o.x,  o.y)) * 2.0; // top-right
	color += texture(textures[inputTexture], uv + vec2(-o.x, -o.y)) * 2.0; // bottom-left
	color += texture(textures[inputTexture], uv + vec2( o.x, -o.y)) * 2.0; // bottom-right

	// Apply bloom strength and normalize by total weight (12)
	outColor = (color / 12.0) * colorMultiplier;
}