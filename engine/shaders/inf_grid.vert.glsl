#version 450 

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    mat4 view;
    mat4 proj;
} scene;

layout(location = 1) out vec3 nearPoint;
layout(location = 2) out vec3 farPoint;

vec3 gridPlane[6] = vec3[](
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0),
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(1, -1, 0)
);

vec3 unproject(vec3 p, mat4 view, mat4 projection)
{
    vec4 unprojected = inverse(view) * inverse(projection) * vec4(p, 1.0);
    return unprojected.xyz / unprojected.w;
}

void main() 
{
    vec3 point = gridPlane[gl_VertexIndex];
    nearPoint = unproject(vec3(point.xy, 0.0), scene.view, scene.proj);
    farPoint = unproject(vec3(point.xy, 1.0), scene.view, scene.proj);

    gl_Position = vec4(point, 1.0);
}
