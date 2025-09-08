#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters : require
#pragma optionNV(fastmath off)

#include "scene.glsl"
#include "mesh.glsl"
#include "bindless.glsl"

layout(push_constant) uniform Constants
{	
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
} constants;

void main()
{	
    mat4 model = constants.modelData.data[gl_DrawID].model;

	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = scene.proj * scene.view * model * vec4(vert.position.xyz, 1.f);
	//gl_Position.z += 0.1; // Additional bias to remove z-fighting
}
