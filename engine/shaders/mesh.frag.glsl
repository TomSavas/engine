#version 460 core
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require
 
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
	// int albedo;
	// int normal;
	mat4 model;
}; 

layout(buffer_reference, std430) readonly buffer ModelDataBuffer
{ 
	ModelData data[];
};

layout(push_constant) uniform Constants
{	
    mat4 model;
    vec4 color;
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
} constants;


layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 color;
layout (location = 4) in vec2 uv;
layout (location = 5) in flat int index;

layout (location = 0) out vec4 outColor;

void main()
{
    // outColor = vec4(1.0, 0.0, 0.0, 1.0);
    const vec3 lightDir = normalize(vec3(1.f, -1.f, 1.f));

    float lDotN = dot(-lightDir, normal);
    const float ambient = 0.1f;
    float lightLevel = min(1.f, max(0.f, lDotN) + ambient);

    // const vec3 color = vec3(1.0, 0.0, 0.0);
    vec3 c = color.xyz;

    vec4 textureIndices = constants.modelData.data[index].textures;
    int index = int(textureIndices.x);
    // int index = constants.modelData.data[index].albedo;
    // int albedoIndex = int(constants.modelData.data[index].textures.x);
    c = texture(textures[index], uv).rgb;
    outColor = vec4(c * lightLevel, color.w);
     // outColor = vec4(normal * 0.5 + 0.5, 1.0);
     // outColor = vec4(vec3(normal.y / 20.f), 1.0);
}
