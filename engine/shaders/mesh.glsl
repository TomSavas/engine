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

struct DefaultMaterial
{
	// albedo
	// normalTexture
	// metallicRoughness
	// bumpTexture
	uvec4 textures;
	vec4 baseColor;
	uvec4 features;
};

uint LIT = 1 << 0;
uint WIREFRAME = 1 << 1;
uint NORMAL_MAPPING = 1 << 2;
uint PARALLAX = 1 << 3;
uint HIGHLIGHT = 1 << 4;
// uint ALL = ~(uint)0;

layout(buffer_reference, std430) readonly buffer Materials
{
	DefaultMaterial materials[];
};

struct Instance
{
	mat4 transform;
	vec4 material;
};

layout(buffer_reference, std430) readonly buffer Instances
{
	Instance instances[];
};

#define DEFINITION(M, T, X) M T X

#define MESH_VS_OUT_COMPONENT(F) \
	F(smooth, vec2, uv);\
	F(flat, int, materialIndex);   \
	F(smooth, vec3, viewPos);             \
	F(smooth, vec3, pos);                 \
	F(smooth, mat3, tbn);                 \
	F(smooth, vec3, tangentCameraPos);    \
	F(smooth, vec3, tangentFragPos);

#define MESH_VS_OUT             \
VertexOutput {                  \
  	MESH_VS_OUT_COMPONENT(DEFINITION) \
}

/*
// #define MESH_VS_OUT           \
// VertexOutput {                \
//   	vec2 uv;                  \
// 	flat int materialIndex;   \
// 	vec3 viewPos;             \
// 	vec3 pos;                 \
// 	mat3 tbn;                 \
// 	vec3 tangentCameraPos;    \
// 	vec3 tangentFragPos;      \
// }
*/
