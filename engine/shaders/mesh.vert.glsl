#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_shading_language_include : require

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
	vec4 enabledFeatures; // normal mapping, parallax mapping
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
	ShadowPassData shadowData;
	Instances instances;
	Materials materials;
} constants;

layout (location = 0) out MESH_VS_OUT vsOut;

void main()
{
	vsOut.objectId = gl_InstanceIndex;
		
    const Instance instance = constants.instances.instances[nonuniformEXT(gl_InstanceIndex)];
    const mat4 model = instance.transform;

	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = scene.proj * scene.view * model * vec4(vert.position.xyz, 1.f);

    vsOut.materialIndex = int(instance.material.x);
    const DefaultMaterial material = constants.materials.materials[vsOut.materialIndex];

    mat3 normalRecalculationMatrix = transpose(inverse(mat3(model)));

    vec3 normal = normalize(normalRecalculationMatrix * vert.normal.xyz);
    vec3 tangent = normalize(normalRecalculationMatrix * vert.tangent.xyz);
    tangent = normalize(tangent - dot(tangent, normal) * normal);
    vec3 bitangent = cross(normal, tangent) * vert.tangent.w;
    vsOut.tbn = mat3(tangent, bitangent, normal);

    vsOut.tangentCameraPos = transpose(vsOut.tbn) * scene.cameraPos.xyz;
    vsOut.tangentFragPos = transpose(vsOut.tbn) * (model * vert.position).xyz;

    vsOut.uv = fma(vert.uv.xy, material.uvScaleOffset.xy, material.uvScaleOffset.zw);
    if ((uint(material.features.x) & MAINTAIN_UV_DENSITY) != 0)
    {
    	vsOut.uv *= vec2(model[0][0], model[1][1]);
	}

    // vsOut.uv = vert.uv.xy;
    vsOut.viewPos = (scene.view * model * vec4(vert.position.xyz, 1.f)).xyz;
    vsOut.pos = (model * vec4(vert.position.xyz, 1.f)).xyz;
}
