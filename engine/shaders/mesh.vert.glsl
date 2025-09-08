#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_shader_draw_parameters : require

#include "scene.glsl"
#include "bindless.glsl"
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
	ModelDataBuffer modelData;
	ShadowPassData shadowData;
	int shadowMapIndex;
} constants;

layout (location = 4) out vec2 uv;
layout (location = 5) out flat int index;
layout (location = 6) out vec3 viewPos;
layout (location = 7) out vec3 pos;
layout (location = 8) out mat3 tbn;
layout (location = 3) out vec3 tangentCameraPos;
layout (location = 2) out vec3 tangentFragPos;

void main()
{
    mat4 model = constants.modelData.data[gl_DrawID].model;

	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = scene.proj * scene.view * model * vec4(vert.position.xyz, 1.f);
    index = gl_DrawID;

    mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));

    vec3 normal = normalize(normalRecalculationMatrix * vert.normal.xyz);
    vec3 tangent = normalize(normalRecalculationMatrix * vert.tangent.xyz);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    vec3 bitangent = cross(normal, tangent) * vert.tangent.w;
    tbn = mat3(tangent, bitangent, normal);

    tangentCameraPos = transpose(tbn) * scene.cameraPos.xyz;
    tangentFragPos = transpose(tbn) * (model * vert.position).xyz;

    uv = vert.uv.xy;
    viewPos = (scene.view * model * vec4(vert.position.xyz, 1.f)).xyz;
    pos = (model * vert.position).xyz;
}
