#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shading_language_include : require

#include "mesh.glsl"

layout(buffer_reference, std430) readonly buffer ShadowPassData
{ 
	mat4 lightViewProj[4];
	mat4 invLightViewProj[4];
	vec4 cascadeDistances[4];
	int cascadeCount;
};

layout(push_constant) uniform Constants
{	
	VertexBuffer vertexBuffer;
	Instances instances;
	ShadowPassData shadowPassData;
	ModelDataBuffer modelData;
	int cascade;
} constants;

void main() 
{	
    const Instance instance = constants.instances.instances[nonuniformEXT(gl_InstanceIndex)];
    const mat4 model = instance.transform;

	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
    gl_Position = constants.shadowPassData.lightViewProj[constants.cascade] * model * vec4(vert.position.xyz, 1.f);
}
