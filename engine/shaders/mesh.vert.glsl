#version 450
#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    mat4 view;
    mat4 proj;
} scene;

struct Vertex 
{
	vec4 position;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

layout(push_constant) uniform Constants
{	
    mat4 model;
    vec4 color;
	VertexBuffer vertexBuffer;
} constants;

layout (location = 1) out vec4 color;

void main() 
{	
	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];

	// gl_Position = scene.proj * scene.view * constants.model * vec4(vert.position.xyz, 1.f);
	gl_Position = constants.model * vec4(vert.position.xyz, 1.f);

    if (vert.position.w > 10.f)
    {
        color = vec4(0.f, 1.f, 0.f, 1.f);
    }
    else 
    {
        color = constants.color;
    }

	// color = vec4(1.0, 0.0, 0.0, 1.0);
	// gl_Position = vec4(vert.position, 1.f);
}
