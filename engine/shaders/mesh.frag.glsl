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

layout(buffer_reference, std430) readonly buffer ShadowPassData
{ 
	mat4 lightViewProj[4];
	mat4 invLightViewProj[4];
	float cascadeDistances[4];
	int cascadeCount;
};

layout(push_constant) uniform Constants
{	
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
	ShadowPassData shadowData;
	int shadowMapIndex;
} constants;


layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 color;
layout (location = 4) in vec2 uv;
layout (location = 5) in flat int index;
layout (location = 6) in vec3 viewPos;
layout (location = 7) in vec3 pos;

layout (location = 0) out vec4 outColor;

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 
);

float textureProj(vec4 shadowCoord, vec2 offset, uint cascadeIndex)
{
	float shadow = 1.f;
	float bias = 0.005;

	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) {
		vec2 uv = shadowCoord.st * vec2(0.25, 1.f) + vec2(0.25 * cascadeIndex, 0.0f);
		float dist = texture(textures[constants.shadowMapIndex], uv).r;
		if (shadowCoord.w > 0 && dist < shadowCoord.z - bias) {
			shadow = .15f;
		}
	}
	return shadow;
}

void main()
{
	uint cascadeIndex = 0;
	for(uint i = 0; i < 3; ++i) {
		if(viewPos.z < constants.shadowData.cascadeDistances[i]) {
			cascadeIndex = i + 1;
		}
	}

	vec4 shadowCoord = (biasMat * constants.shadowData.lightViewProj[cascadeIndex]) * vec4(pos, 1.0);
	// // float shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
	float shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
	//float shadow = 1.f;
		
    // outColor = vec4(1.0, 0.0, 0.0, 1.0);
    const vec3 lightDir = normalize(vec3(1.f, -1.f, 1.f));

    float lDotN = dot(-lightDir, normal);
    const float ambient = 0.1f;
    float lightLevel = min(1.f, max(0.f, lDotN) + ambient);

    // const vec3 color = vec3(1.0, 0.0, 0.0);
    vec3 c = color.xyz;

    vec4 textureIndices = constants.modelData.data[index].textures;
    int index = int(textureIndices.x);
    // // int index = constants.modelData.data[index].albedo;
    // // int albedoIndex = int(constants.modelData.data[index].textures.x);
    c = texture(textures[index], uv).rgb;
    // c = vec3(index % 15, index % 30, index % 45);
    outColor = vec4(c * lightLevel * shadow, color.w);
    // outColor = vec4(c * lightLevel, color.w);

 //    if (constants.color.r > 0.0)
 //    {
	//     switch(cascadeIndex) 
	// 	{
	// 		case 0 : 
	// 			outColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
	// 			break;
	// 		case 1 : 
	// 			outColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
	// 			break;
	// 		case 2 : 
	// 			outColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
	// 			break;
	// 		case 3 : 
	// 			outColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
	// 			break;
	// 	}
	// }
     // outColor = vec4(normal * 0.5 + 0.5, 1.0);
     // outColor = vec4(vec3(normal.y / 20.f), 1.0);
}
