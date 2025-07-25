float remap(float value, float fromRangeStart, float fromRangeEnd, float toRangeStart, float toRangeEnd) {
    return toRangeStart + (value - fromRangeStart) * (toRangeEnd - toRangeStart) / (fromRangeEnd - fromRangeStart);
}

vec3 ndcToView(vec3 ndc, mat4 invProj)
{
    vec4 clip = vec4(ndc.xyz, 1.0f);
    vec4 view = invProj * clip;
    return view.xyz / view.w;
}

struct Plane
{
    // This is in terms of plane equation: n dot P = d
    vec3 n;
    float d;
};

Plane planeFromPoints(vec3 a, vec3 b, vec3 c)
{
    // This expects counter-clockwise winding of vertices
    Plane p;
    p.n = normalize(cross(b - a, c - a));
    p.d = dot(p.n, a);

    return p;
}
