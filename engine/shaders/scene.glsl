layout(set = 0, binding = 0) uniform SceneUniforms
{
    vec4 cameraPos;
    mat4 view;
    mat4 proj;
    vec3 lightDir;
} scene;