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

layout (location = 1) out vec4 geomColor;
layout (location = 2) out vec3 normal;
layout (location = 3) out vec4 color;
layout (location = 4) out vec2 uv;
layout (location = 5) out flat int index;
layout (location = 6) out vec3 viewPos;
layout (location = 7) out vec3 pos;

void main()
{	
	Vertex vert = constants.vertexBuffer.vertices[gl_VertexIndex];
	gl_Position = scene.proj * scene.view * vec4(vert.position.xyz, 1.f);
	// gl_Position = constants.model * vec4(vert.position.xyz, 1.f);

    // geomColor = constants.color;
    // color = constants.color;
    geomColor = vec4(1.0, 0.0, 0.0, 1.0);
    color = vec4(1.0, 0.0, 0.0, 1.0);

    normal = vert.normal.xyz;

    index = gl_BaseInstance;
    index = gl_DrawID;

    // uv = constants.modelData.data[gl_DrawID];
    uv = vert.uv.xy;

    viewPos = (scene.view * vec4(vert.position.xyz, 1.f)).xyz;
    // viewPos = (scene.proj * scene.view * constants.model * vec4(vert.position.xyz, 1.f)).xyz;
    pos = vert.position.xyz;

	// color = vec4(1.0, 0.0, 0.0, 1.0);
	// gl_Position = vec4(1.0, 2.0, 3.0, 1.0);
}
