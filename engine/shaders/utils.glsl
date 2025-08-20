float remap(float value, float fromRangeStart, float fromRangeEnd, float toRangeStart, float toRangeEnd) {
    return toRangeStart + (value - fromRangeStart) * (toRangeEnd - toRangeStart) / (fromRangeEnd - fromRangeStart);
}

vec3 deproject(vec3 point, mat4 inv)
{
    vec4 p = vec4(point, 1.0f);
    vec4 deprojected = inv * p;
    return deprojected.xyz / deprojected.w;
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

// Collisions

struct Hit
{
    bool hit;
    float t0;
    float t1;
};

Hit raySphereIntersection(vec3 point, vec3 rayDir, vec3 sphereCenter, float radius)
{
    vec3 toSphereOrigin = point - sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.f * dot(rayDir, toSphereOrigin);
    float c = dot(toSphereOrigin, toSphereOrigin) - radius * radius;

    float discriminant = b * b - 4.f * a * c;
    if (discriminant < 0.f)
        return Hit(false, 0.f, 0.f);

    float t0 = (-b + sqrt(discriminant)) / (2.f * a);
    float t1 = (-b - sqrt(discriminant)) / (2.f * a);

    return Hit(true, min(t0, t1), max(t0, t1));
}





