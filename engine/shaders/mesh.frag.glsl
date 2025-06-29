#version 460
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

#define PCF_TAPS_PER_AXIS 3

float shadowIntensity(mat4 viewProj, uint cascadeIndex, float cascadeCount) {
    vec4 lightSpacePos = viewProj * vec4(pos, 1.f);
    lightSpacePos = lightSpacePos / lightSpacePos.w;
    vec2 lightSpacePosNDC = lightSpacePos.xy * 0.5f + 0.5f;

    float shadow = 0.f;

    // Add directional bias
	float bias = 0.01f;

    float cascadeURange = 1.f / cascadeCount;
	vec2 cascadeNDC = lightSpacePosNDC.xy * vec2(cascadeURange, 1.f) + vec2(cascadeURange * cascadeIndex, 0.f);

    //float shadowDepth = texture(textures[constants.shadowMapIndex], cascadeNDC).r;
    //if (shadowDepth < (lightSpacePos.z - bias))
    //{
    //    shadow = 0.85f;
    //}

    vec2 texelSize = 1.f / textureSize(textures[constants.shadowMapIndex], 0) / vec2(cascadeCount, 1.f);
    for (int x = -PCF_TAPS_PER_AXIS / 2; x <= PCF_TAPS_PER_AXIS / 2; x++)
    {
        for (int y = -PCF_TAPS_PER_AXIS / 2; y <= PCF_TAPS_PER_AXIS / 2; y++)
        {
            float shadowDepth = texture(textures[constants.shadowMapIndex], cascadeNDC + vec2(x, y) * texelSize).r;
            shadow += (lightSpacePos.z - bias) > shadowDepth ? 0.85 : 0.0;
        }
    }
    shadow = shadow / float(PCF_TAPS_PER_AXIS * PCF_TAPS_PER_AXIS);

    return shadow;
}

void main()
{
	uint cascadeIndex = 0;
	for(uint i = 0; i < constants.shadowData.cascadeCount - 1; ++i) {
		if(viewPos.z < constants.shadowData.cascadeDistances[i].x) {
			cascadeIndex = i + 1;
		}
	}

	////vec4 shadowCoord = (biasMat * constants.shadowData.lightViewProj[cascadeIndex]) * vec4(pos, 1.0);
	//vec4 shadowCoord = (constants.shadowData.lightViewProj[cascadeIndex]) * vec4(pos, 1.0);
	//float shadow = textureProj(shadowCoord / shadowCoord.w, vec2(0.0), cascadeIndex);
	float shadow = shadowIntensity(constants.shadowData.lightViewProj[cascadeIndex], cascadeIndex, float(constants.shadowData.cascadeCount));
		
    const vec3 lightDir = normalize(vec3(1.f, -1.f, 1.f));

    float lDotN = dot(-lightDir, normal);
    const float ambient = 0.1f;
    float lightLevel = min(1.f, max(0.f, lDotN) + ambient);

    vec3 c = color.xyz;

    vec4 textureIndices = constants.modelData.data[index].textures;
    int index = int(textureIndices.x);
    // // int index = constants.modelData.data[index].albedo;
    // // int albedoIndex = int(constants.modelData.data[index].textures.x);
    c = texture(textures[index], uv).rgb;
    // c = vec3(index % 15, index % 30, index % 45);
    float unshadowedLevel = 1.f - shadow;
    outColor = vec4(c * lightLevel * unshadowedLevel, color.w);
    // outColor = vec4(c * lightLevel, color.w);

	//switch(cascadeIndex)
	//{
	//	case 0 :
	//		outColor.rgb *= vec3(1.0f, 0.25f, 0.25f);
	//		break;
	//	case 1 :
	//		outColor.rgb *= vec3(0.25f, 1.0f, 0.25f);
	//		break;
	//	case 2 :
	//		outColor.rgb *= vec3(0.25f, 0.25f, 1.0f);
	//		break;
	//	case 3 :
	//		outColor.rgb *= vec3(1.0f, 1.0f, 0.25f);
	//		break;
	//}
}
