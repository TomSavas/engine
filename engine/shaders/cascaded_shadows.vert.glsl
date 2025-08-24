#version 460
#extension GL_EXT_buffer_reference : require

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

layout(buffer_reference, std430) readonly buffer ShadowPassData
{ 
	mat4 lightViewProj[4];
	mat4 invLightViewProj[4];
	vec4 cascadeDistances[4];
	int cascadeCount;
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
	ShadowPassData shadowPassData;
	ModelDataBuffer modelData;
	int cascade;
} constants;

void main() 
{	
    mat4 model = constants.modelData.data[gl_DrawID].model;

	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = constants.shadowPassData.lightViewProj[constants.cascade] * model * vec4(vert.position.xyz, 1.f);
}
