#version 460

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 normal;
layout (location = 2) in vec3 color;
layout (location = 3) in vec2 uv;

layout (location = 0) out vec3 outColor;
layout (location = 1) out vec2 outUv;

layout (set = 0, binding = 0) uniform CameraBuffer {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
} cameraData;

struct ObjectData {
    mat4 modelMatrix;
};

layout (std140, set = 1, binding = 0) readonly buffer ObjectDataBuffer {
    ObjectData[] data;
} objectBuffer;

void main()
{
    gl_Position = objectBuffer.data[gl_BaseInstance].modelMatrix * vec4(position, 1.0f);
    outColor = color;
    outUv = uv;
}
