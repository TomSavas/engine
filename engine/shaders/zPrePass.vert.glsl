#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters : require
#pragma optionNV(fastmath off)

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 0) uniform sampler2D textures[]; 

struct Vertex 
{
	vec4 position;
	vec4 uv;
	vec4 normal;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

struct ModelData 
{
	vec4 textures;
	mat4 model;
}; 

layout(buffer_reference, std430) readonly buffer ModelDataBuffer
{ 
	ModelData data[];
};

layout(push_constant) uniform Constants
{	
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
} constants;

void main()
{	
	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = scene.proj * scene.view * vec4(vert.position.xyz, 1.f);
	gl_Position.z += 0.1; // Additional bias to remove z-fighting
}
