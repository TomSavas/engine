#version 460

layout(push_constant) uniform Constants
{
    // TODO: convert to specialisation constant
    vec4 depth;
    vec4 sunDir;
} constants;

layout (location = 0) out vec2 uv;

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

	gl_Position = vec4(positions[gl_VertexIndex], constants.depth.x, 1.0f);
	uv = uvs[gl_VertexIndex];
}
