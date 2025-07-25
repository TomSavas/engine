#version 460

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    vec4 cameraPos;
    mat4 view;
    mat4 proj;
} scene;

layout (location = 0) out vec3 outColor;

void main() 
{
	const vec3 positions[3] = vec3[3](
		vec3( 1.f, -1.f, 0.0f),
		vec3(-1.f, -1.f, 0.0f),
		vec3( 0.f,  1.f, 0.0f)
	);

	const vec3 colors[3] = vec3[3](
		vec3(1.0f, 0.0f, 0.0f),
		vec3(0.0f, 1.0f, 0.0f),
		vec3(0.0f, 0.0f, 1.0f)
	);

	gl_Position = scene.proj * scene.view * vec4(positions[gl_VertexIndex], 1.0f);
	outColor = colors[gl_VertexIndex];
}

