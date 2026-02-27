#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shading_language_include : require

#include "lights.glsl"
#include "utils.glsl"
#include "sdf.glsl"
#include "scene.glsl"
#include "pbr.glsl"
#include "debug_utils.glsl"

layout(depth_any) out float gl_FragDepth;

layout(set = 1, binding = 0) uniform sampler2D textures[];

// layout(push_constant) uniform Constants
// {
// };

layout (location = 0) in vec2 uv;
layout (location = 1) in vec3 pos;

layout (location = 0) out vec4 outColor;

float de(vec3 p0){
    vec4 p = vec4(p0, 1.f);
    for (int i = 0; i < 8; i++){
        p.xyz = mod(p.xyz - 1.f, 2.f) - 1.f;
        p *= 1.4f / dot(p.xyz, p.xyz);
    }
    return (length(p.xz / p.w) * 0.25f);
}

// float de ( vec3 p ) {
// p = mod( p, 2.0f ) - 1.0f;
// p = abs( p ) - 1.0f;
// if ( p.x < p.z ) p.xz = p.zx;
// if ( p.y < p.z ) p.yz = p.zy;
// if ( p.x < p.y ) p.xy = p.yx;
// float s = 1.0f;
// for ( int i = 0; i < 10; i++ ) {
//   float r2 = 2.0f / clamp( dot( p, p ), 0.1f, 1.0f );
//   p = abs( p ) * r2 - vec3( 0.6f, 0.6f, 3.5f );
//   s *= r2;
// }
// return length( p ) / s;
// }

sdfResult fractal(vec3 p)
{
    sdfResult r;
    float scale = 100.f;
    r.dist = de(p/scale) * scale;
    r.color = heatmapGradient(fract(dot(p,p) / 50.f));
    return r;
}

sdfResult testScene(vec3 p)
{
    p -= vec3(0.f, 0.f, 30.f);
    // 
    // sdfResult s = sphere(p, 5.f);
    // s.color = vec3(1.f, 0.f, 0.f);
    // return s;
    sdfResult s = sphere(p, 5.f * (sin(scene.time.x) * 0.5 + 0.5 + 0.25));
    sdfResult b = box(p - vec3(3.f, 0.f, 0.f), vec3(5.f, 2.f, 2.f));

    float width = 0.5f;
    float factor = clamp(0.5 + (s.dist - b.dist) / (2.0 * width), 0.0, 1.0);

    sdfResult r;
    r.dist = sdfSmoothUnion(s, b, 0.5f);
    r.normal = mix(s.normal, b.normal, factor);
    r.color = mix(vec3(1.f, 0.f, 0.f), vec3(0.f, 0.f, 1.f), factor);

    {
        s = sphere(p - vec3(9, cos(scene.time.x) * 5.f, 2.f), 1);
        width *= 2.f;
        factor = clamp(0.5 + (r.dist - s.dist) / (2.0 * width), 0.0, 1.0);

        r.dist = sdfSmoothUnion(r, s, 0.5f);
        r.normal = mix(r.normal, s.normal, factor);
        r.color = mix(r.color, vec3(0.f, 1.f, 0.f), factor);
    }

    {
        s = cylinder(p + vec3(2.f, 0.f, 0.f) - vec3(10.f, 0.f, 0.f) * (sin(scene.time.x * PI) * 0.5f + 0.5f), vec3(0.f, 0.f, -7.5f), vec3(0.f, 0.f, 15.f), 0.5f);
        width /= 2.f;
        factor = clamp(0.5 + (r.dist - s.dist) / (2.0 * width), 0.0, 1.0);

        r.dist = sdfSmoothSubtraction(s, r, 0.25f);
        r.normal = mix(r.normal, s.normal, factor);
        r.color = mix(r.color, vec3(1.f, 1.f, 1.f), factor);
    }

    return r;
}

#define SCENE testScene

vec3 calcNormal(vec3 p)
{
    const float h = 0.0001;
    const vec2 k = vec2(1,-1);
    return normalize( k.xyy*SCENE( p + k.xyy*h ).dist + 
                      k.yyx*SCENE( p + k.yyx*h ).dist + 
                      k.yxy*SCENE( p + k.yxy*h ).dist + 
                      k.xxx*SCENE( p + k.xxx*h ).dist );
}

sdfResult raymarch(vec3 initialPos, vec3 dir)
{
    sdfResult result;
    result.dist = 0;
    vec3 pos = initialPos;
        
#define MAX_STEPS 128
#define MAX_DIST 100.f
    float t = 0;
    for (int i = 0; i < MAX_STEPS && t < MAX_DIST; ++i)
    {
        result = SCENE(pos);
        if (result.dist < 0.001)
        {
            result.normal = calcNormal(pos);
            // result.color = heatmapGradient(fract(t * 5.f));
            result.dist = t;
            return result;       
        }
        t += result.dist;

        pos = initialPos + dir * t;
    }

    result.dist = -1;
    return result;
}

void main()
{
    vec4 rayEye = inverse(scene.view) * inverse(scene.proj) * vec4(uv * 2.0 - 1.f, 1.0, 1.0);
    vec3 a = rayEye.xyz / rayEye.w;

    vec3 rayDir = normalize(a - scene.cameraPos.xyz);

    // vec4 target = inverse(scene.proj) * vec4(uv * 2.0 - 1.0, 1.0, 1.0);
    // vec3 rayDir = (inverse(scene.view) * vec4(normalize(target.xyz), 0.0)).xyz;

    sdfResult result = raymarch(scene.cameraPos.xyz, rayDir);

    if (result.dist > 0)
    {
        vec3 f0 = vec3(0.04);
        f0 = mix(f0, result.color, 0.5);

        vec3 c = pbrLight(
            vec3(-scene.lightDirIntensity.x, -scene.lightDirIntensity.y, scene.lightDirIntensity.z),
            scene.lightDirIntensity.w,
            rayDir,
            result.normal,
            0,
            result.color,
            vec2(0.5, 0.5),
            f0
        );
        c = c + result.color * 0.005;
        outColor = vec4(c, 1.f);

        vec4 point = scene.proj * scene.view * vec4(scene.cameraPos.xyz + rayDir * result.dist, 1.f);
        float depth = point.z / point.w;
        gl_FragDepth = clamp(depth, 0.f, 1.f);
    }
    else
    {
        discard;
    }
}
