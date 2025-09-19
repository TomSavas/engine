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
	vec4 textures; // albedo, normal, roughness
	vec4 selected; // debug only
	vec4 metallicRoughnessFactors;
	mat4 model;
};

layout(buffer_reference, std430) readonly buffer ModelDataBuffer
{
	ModelData data[];
};
