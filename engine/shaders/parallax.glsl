vec2 parallaxMap(vec2 vertUv, int bumpMapIndex)
{
	const float heightScale = 0.01f;
	vec3 viewDir = normalize(tangentCameraPos - tangentFragPos);

    float height = 1.f - texture(textures[bumpMapIndex], vertUv).r;
    vec2 p = viewDir.xy / viewDir.z * (height * heightScale); // /z is for making it look better at oblique angles
    return vertUv - p;
}

vec2 steepParallaxMap(vec2 vertUv, int bumpMapIndex)
{
	const float heightScale = 0.1f;
	vec3 viewDir = normalize(tangentCameraPos - tangentFragPos);

    const float numLayers = 100.f;
    float layerDepth = 1.f / numLayers;
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    // get initial values
	vec2 uv = vertUv;
    float currentDepthMapValue = 1.f - texture(textures[bumpMapIndex], uv).r;

	while(currentLayerDepth < currentDepthMapValue)
	{
	    uv -= deltaTexCoords;
	    currentDepthMapValue = 1.f - texture(textures[bumpMapIndex], uv).r;
	    currentLayerDepth += layerDepth;
	}

	return uv;
}

vec2 parallaxOcclussionMap(vec2 vertUv, int bumpMapIndex)
{
	const float heightScale = 0.025f;
	vec3 viewDir = tangentCameraPos - tangentFragPos;

	float viewDirLen = length(viewDir);
    const float minDist = 0;
    const float maxDist = 150;
    float distLayerMultiplier = clamp(remap(viewDirLen, minDist, maxDist, 0, 1), 0.f, 1.f);
    distLayerMultiplier = 1.f - distLayerMultiplier;

	viewDir = normalize(viewDir);

    const float minLayers = 2;
    const float maxLayers = 128;
    //float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)) * distLayerMultiplier);
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));

    float layerDepth = 1.0 / numLayers;
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    // get initial values
	vec2 uv = vertUv;
    float currentDepthMapValue = 1.f - texture(textures[bumpMapIndex], uv).r;

	while(currentLayerDepth < currentDepthMapValue)
	{
	    // shift texture coordinates along direction of P
	    uv -= deltaTexCoords;
	    // get depthmap value at current texture coordinates
	    //currentDepthMapValue = texture(depthMap, currentTexCoords).r;
	    currentDepthMapValue = 1.f - texture(textures[bumpMapIndex], uv).r;
	    // get depth of next layer
	    currentLayerDepth += layerDepth;
	}

	vec2 prevTexCoords = uv + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = (1.f - texture(textures[bumpMapIndex], uv).r) - currentLayerDepth + layerDepth;

	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + uv * (1.0 - weight);

	return finalTexCoords;
}

vec2 parallaxOcclussionMapBinarySearch(vec2 vertUv, int bumpMapIndex)
{
	const float heightScale = 0.025f;
	vec3 viewDir = normalize(tangentCameraPos - tangentFragPos);

    const float numLayers = 8.f;
    float layerDepth = 1.f / numLayers;
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * heightScale;
    vec2 deltaTexCoords = P / numLayers;

    // get initial values
	vec2 uv = vertUv;
    float currentDepthMapValue = 1.f - texture(textures[bumpMapIndex], uv).r;

	while(currentLayerDepth < currentDepthMapValue)
	{
	    uv -= deltaTexCoords;
	    currentDepthMapValue = 1.f - texture(textures[bumpMapIndex], uv).r;
	    currentLayerDepth += layerDepth;
	}

	float prevDepth = currentLayerDepth - layerDepth;
	vec2 prevUv = uv + deltaTexCoords;

    // Binary search to find the exact intersection
	float midDepth = (currentLayerDepth + prevDepth) / 2.f;
	vec2 midUv = (uv + prevUv) / 2.f;
	float midReadDepth = 1.f - texture(textures[bumpMapIndex], midUv).r;
	uint iterationLimit = 8;
	while (abs(midReadDepth - midDepth) > 0.0001f && iterationLimit > 0)
	{
	    iterationLimit -= 1;

	    if (midReadDepth - midDepth > 0.f)
	    {
            prevDepth = midDepth;
            prevUv = midUv;
	    }
	    else
	    {
            currentLayerDepth = midDepth;
            uv = midUv;
	    }

        midDepth = (currentLayerDepth + prevDepth) / 2.f;
        midUv = (uv + prevUv) / 2.f;
        midReadDepth = 1.f - texture(textures[bumpMapIndex], midUv).r;
	}

	return midUv;
}