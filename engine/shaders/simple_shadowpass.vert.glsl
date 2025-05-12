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
	float cascadeDistances[4];
	int cascadeCount;
};

layout(push_constant) uniform Constants
{	
    // mat4 lightViewProj;
	VertexBuffer vertexBuffer;
	ShadowPassData shadowPassData;
	int cascade;
} constants;

void main() 
{	
	// Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
 //    gl_Position = constants.lightViewProj * vec4(vert.position.xyz, 1.f);        
	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = constants.shadowPassData.lightViewProj[constants.cascade] * vec4(vert.position.xyz, 1.f);        
}
