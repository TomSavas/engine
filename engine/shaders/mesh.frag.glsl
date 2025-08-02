#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require

#include "debug_utils.glsl"
#include "lights.glsl"
#include "utils.glsl"

layout(origin_upper_left) in vec4 gl_FragCoord;

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    vec4 cameraPos;
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
	Lights lights;
	LightIds lightIds;
	LightTileData lightTiles;
	int shadowMapIndex;
} constants;


//layout (location = 2) in vec3 normal;
//layout (location = 3) in vec4 color;
layout (location = 4) in vec2 vert_uv;
layout (location = 5) in flat int index;
layout (location = 6) in vec3 viewPos;
layout (location = 7) in vec3 pos;
layout (location = 8) in mat3 tbn;
layout (location = 3) in vec3 tangentCameraPos;
layout (location = 2) in vec3 tangentFragPos;

layout (location = 0) out vec4 outColor;

#include "parallax.glsl"

const mat4 biasMat = mat4( 
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0
);

#define PCF_TAPS_PER_AXIS 15

float shadowIntensity(mat4 viewProj, uint cascadeIndex, float cascadeCount)
{
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
            shadow += (lightSpacePos.z - bias) > shadowDepth ? 1.f : 0.f;
        }
    }
    shadow = shadow / float(PCF_TAPS_PER_AXIS * PCF_TAPS_PER_AXIS);
    //shadow = shadow / 121.f;

    return shadow;
}

vec3 calculateDiffuse(vec3 color, vec3 normal, vec3 lightDir, float cosTheta) {
    return color * cosTheta;
}

vec3 calculateSpecular(vec3 color, vec3 normal, vec3 lightDir, vec3 cameraDir, float cosTheta, float specularExp) {
    float specularIntensity = pow(max(0.f, dot(cameraDir, reflect(-lightDir, normal))), specularExp);
    return color * specularIntensity;
}

float linearizeDepth(float depth, float near, float far)
{
    //depth = 2.f * depth - 1.f;
    //return 2.f * near * far / (far + near - depth * (far - near));


    //return near * far / (far + depth * (far - near));
    //return near * far / (far + near - depth * (far - near));
    return near * far / (far - depth * (far - near));
}

float linearizeDepthFromCameraParams(float depth)
{
    //return linearizeDepth(depth, nearFarPlanes.x, nearFarPlanes.y);
    return linearizeDepth(depth, 0.1f, 5000.f);
}

void main()
{
    const vec4 textureIndices = constants.modelData.data[index].textures;
    const int index = int(textureIndices.x);
    vec2 uv = parallaxOcclussionMapBinarySearch(vert_uv, int(textureIndices.z));

	uint cascadeIndex = 0;
	for(uint i = 0; i < constants.shadowData.cascadeCount - 1; ++i) {
		if(viewPos.z < constants.shadowData.cascadeDistances[i].x) {
			cascadeIndex = i + 1;
		}
	}

	float shadow = shadowIntensity(constants.shadowData.lightViewProj[cascadeIndex], cascadeIndex, float(constants.shadowData.cascadeCount));

    vec3 texNormal = texture(textures[int(textureIndices.y)], uv).rgb;
	vec3 n = normalize(tbn * (texNormal * vec3(2.f) - vec3(1.f)));

    vec3 cameraDir = normalize(scene.cameraPos.xyz - pos);
    vec3 diffuse = vec3(0.f);
    vec3 specular = vec3(0.f);

    // Look up light tile
    vec2 fragmentPos = gl_FragCoord.xy / vec2(1920.f, 1080.f);
    vec2 lightTileId = fragmentPos * vec2(96.f, 54.f);
    int tileIndex = int(lightTileId.y) * 96 + int(lightTileId.x);
    uint lightCount = constants.lightTiles.tiles[tileIndex].count;
    uint lightOffset = constants.lightTiles.tiles[tileIndex].offset;

    for (uint i = 0; i < lightCount; i++)
    {
        uint lightIndex = constants.lightIds.ids[i + lightOffset];
        PointLight light = constants.lights.pointLights[lightIndex];
        vec3 lightDir = light.pos.xyz - pos;
        vec3 normLightDir = normalize(lightDir);
        float dist = length(lightDir);
        float radius = light.range.x;

        float normalizedDist = (radius - dist) / radius;
        float strength = pow(max(normalizedDist, 0.f), 2.f);

        float cosTheta = max(0.f, dot(normLightDir, n));

        diffuse += calculateDiffuse(light.color.rgb, n, normLightDir, cosTheta) * strength * 7.f;
        specular += calculateSpecular(light.color.rgb, n, normLightDir, cameraDir, cosTheta, 8.f) * strength * 7.f;
    }

    vec3 c = texture(textures[index], uv).rgb;

    const vec3 lightDir = normalize(vec3(0.6f, -1.0f, 0.1f));
    const float ambient = 0.02f;
    const float dirIntensity = 1.f;

    float unshadowedLevel = 1.f - shadow;
    //float unshadowedLevel = 1.f;
    diffuse += max(0.f, dot(-lightDir, n)) * dirIntensity * unshadowedLevel;

    // Add tonemapping later, clamp for now
    //vec3 light = clamp(vec3(0.f), vec3(1.f), diffuse + specular + vec3(ambient));
    //vec3 light = max(vec3(0.f), diffuse + specular + vec3(ambient));
    //vec3 light = max(vec3(0.f), diffuse + specular + vec3(ambient));
    //vec3 light = max(vec3(0.f), diffuse + vec3(ambient));
    vec3 light = max(vec3(0.f), diffuse + specular + vec3(ambient));
    //light = vec3(1.f);
    //outColor = vec4(c * light * heatmapGradient(0.f), 1.f);

    //outColor = vec4(fragmentPos.xy, 0.f, 1.f);
    //outColor = vec4(lightTileId, 0.f, 1.f);
    //outColor = vec4(vec3(float(lightCount) / 60.f), 1.f);

    outColor = vec4(c * light, 1.f);
    //outColor = vec4(c * light * heatmapGradient(float(lightCount) / 32.f), 1.f);

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
