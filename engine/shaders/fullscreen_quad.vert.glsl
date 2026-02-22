#version 460
#extension GL_ARB_shading_language_include : require

#include "scene.glsl"

layout(push_constant) uniform Constants
{
    // TODO: convert to specialisation constant
    vec4 fullscreenQuadDepth;
};

layout (location = 0) out vec2 uv;
layout (location = 1) out vec3 pos;

void main()
{
	const vec2 positions[3] = vec2[3](
		vec2(-1.f, -1.f),
		vec2( 3.f, -1.f),
		vec2(-1.f,  3.f)
	);
	const vec2 uvs[3] = vec2[3](
		vec2(0.f, 0.f),
		vec2(2.f, 0.f),
		vec2(0.f, 2.f)
	);

	vec4 ndcPos = vec4(positions[gl_VertexIndex], 0.99f, 1.0f);
	gl_Position = ndcPos;

	uv = uvs[gl_VertexIndex];
	vec4 p = ndcPos * inverse(scene.proj * scene.view);
	pos = p.xyz / p.w;
}
