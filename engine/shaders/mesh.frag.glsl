#version 450
#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    mat4 view;
    mat4 proj;
} scene;

layout (location = 2) in vec3 normal;
layout (location = 3) in vec4 color;

layout (location = 0) out vec4 outColor;

void main()
{
    // outColor = vec4(1.0, 0.0, 0.0, 1.0);
    const vec3 lightDir = normalize(vec3(1.f, -1.f, 1.f));

    float lDotN = dot(-lightDir, normal);
    const float ambient = 0.1f;
    float lightLevel = min(1.f, max(0.f, lDotN) + ambient);

    // const vec3 color = vec3(1.0, 0.0, 0.0);
    outColor = vec4(color.xyz * lightLevel, color.w);
     // outColor = vec4(normal * 0.5 + 0.5, 1.0);
     // outColor = vec4(vec3(normal.y / 20.f), 1.0);
}
