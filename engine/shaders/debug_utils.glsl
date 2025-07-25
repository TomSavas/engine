vec3 heatmapGradient(float percentage)
{
#define GRADIENT_COLOR_COUNT 6
    const vec3 gradient[GRADIENT_COLOR_COUNT] = vec3[]
        (
             vec3(0.f, 0.f, 0.1f),
             vec3(0.f, 0.f, 0.8f),
             vec3(0.f, 0.6f, 0.6f),
             vec3(0.f, 0.8f, 0.f),
             vec3(0.8f, 0.8f, 0.f),
             vec3(0.8f, 0.f, 0.f)
        );

    vec3 color = gradient[0];
    for (int i = 1; i < GRADIENT_COLOR_COUNT; i++)
    {
        const float multiple = GRADIENT_COLOR_COUNT - 1;
        color = mix(color, gradient[i] * 3.f, clamp((percentage - float(i - 1) * 1.f/multiple) * multiple, 0.f, 1.f));
    }

    return color;
}