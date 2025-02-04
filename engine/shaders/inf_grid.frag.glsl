#version 450

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    mat4 view;
    mat4 proj;
} scene;

layout(location = 1) in vec3 nearPoint;
layout(location = 2) in vec3 farPoint;

layout(location = 0) out vec4 outColor;

vec4 calcGridColour(vec3 point, float scale, float intensity)
{
    vec2 scaledPoint = point.xz * scale;
    vec2 derivatives = fwidth(scaledPoint);
    vec2 grid = abs(fract(scaledPoint - 0.5) - 0.5) / derivatives; 

    float invLine = min(grid.x, grid.y);
    float line = 1.0 - min(invLine, 1.0);

    vec4 color = vec4(vec3(intensity * line), 1.0);

    vec2 clampedDerivatives = min(derivatives, 1.0);
    if (-0.2 * clampedDerivatives.x < point.x && point.x < 0.2 * clampedDerivatives.x)
        color.rgb = vec3(0.0, 0.0, 1.0);
    if (-0.2 * clampedDerivatives.y < point.z && point.z < 0.2 * clampedDerivatives.y)
        color.rgb = vec3(1.0, 0.0, 0.0);
        
    return color;
}

float computeLinearDepth(vec3 pos)
{
    const float near = 0.1;
    const float far = 10000.0;

    vec4 clip_space_pos = scene.proj * scene.view * vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * near * far) / (far + near - clip_space_depth * (far - near)); // get linear value between 0.01 and 100
    return linearDepth / far; // normalize
}

void main() 
{
    float t = -nearPoint.y / (farPoint.y - nearPoint.y);
    vec3 point = nearPoint + t * (farPoint - nearPoint);

    float scale = 10.0;
    float intensity = 0.01;
    vec4 gridColor = vec4(0.0);
    for (int i = 0; i < 4; i++)
    {
        gridColor = max(gridColor, calcGridColour(point, scale, intensity));
        scale /= 10.0;
        intensity *= 4.0;
    }

    float linearDepth = computeLinearDepth(point);
    float fading = max(0.0, 1.0 - (50.0 * linearDepth));
    fading = pow(fading, 5);

    outColor = vec4(gridColor.rgb, gridColor.a * float(t > 0) * fading);
}
