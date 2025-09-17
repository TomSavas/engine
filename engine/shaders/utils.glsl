float remap(float value, float fromRangeStart, float fromRangeEnd, float toRangeStart, float toRangeEnd) {
    return toRangeStart + (value - fromRangeStart) * (toRangeEnd - toRangeStart) / (fromRangeEnd - fromRangeStart);
}

vec3 deproject(vec3 point, mat4 inv)
{
    const vec4 p = vec4(point, 1.0f);
    const vec4 deprojected = inv * p;
    return deprojected.xyz / deprojected.w;
}

vec3 ndcToView(vec3 ndc, mat4 invProj)
{
    const vec4 clip = vec4(ndc.xyz, 1.0f);
    const vec4 view = invProj * clip;
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

float linearizeDepth(float depth, float near, float far)
{
    return near * far / (far + depth * (near - far));
}

float distanceSquared(vec2 a, vec2 b)
{
    const vec2 distVec = a - b;
    return dot(distVec, distVec);
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
    const vec3 toSphereOrigin = point - sphereCenter;
    const float a = dot(rayDir, rayDir);
    const float b = 2.f * dot(rayDir, toSphereOrigin);
    const float c = dot(toSphereOrigin, toSphereOrigin) - radius * radius;

    const float discriminant = b * b - 4.f * a * c;
    if (discriminant < 0.f)
        return Hit(false, 0.f, 0.f);

    const float t0 = (-b + sqrt(discriminant)) / (2.f * a);
    const float t1 = (-b - sqrt(discriminant)) / (2.f * a);

    return Hit(true, min(t0, t1), max(t0, t1));
}





