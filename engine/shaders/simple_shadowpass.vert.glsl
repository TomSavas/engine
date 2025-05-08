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

layout(push_constant) uniform Constants
{	
    mat4 lightViewProj;
	VertexBuffer vertexBuffer;
} constants;

void main() 
{	
	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = constants.lightViewProj * vec4(vert.position.xyz, 1.f);        
}
