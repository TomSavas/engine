// Primitives
struct sdfResult
{
    float dist;  

    vec3 normal;
    vec3 color;
};

sdfResult sphere(vec3 p, float radius)
{
    sdfResult r;
    r.dist = length(p) - radius;        

    return r;
}

sdfResult plane(vec3 p, vec3 normal, float h)
{
    sdfResult r;
    r.dist = dot(p, normal) + h;

    return r;
}

sdfResult box(vec3 p, vec3 bounds)
{
    sdfResult r;

    vec3 q = abs(p) - bounds;
    r.dist = length(max(q,0.0)) + min(max(q.x,max(q.y,q.z)),0.0);

    return r;
}

sdfResult cylinder(vec3 p, vec3 a, vec3 b, float r)
{
  vec3  ba = b - a;
  vec3  pa = p - a;
  float baba = dot(ba,ba);
  float paba = dot(pa,ba);
  float x = length(pa*baba-ba*paba) - r*baba;
  float y = abs(paba-baba*0.5)-baba*0.5;
  float x2 = x*x;
  float y2 = y*y*baba;
  float d = (max(x,y)<0.0)?-min(x2,y2):(((x>0.0)?x2:0.0)+((y>0.0)?y2:0.0));

  sdfResult res;
  res.dist = sign(d)*sqrt(abs(d))/baba;
  return res;
}

// Operations
float sdfUnion(sdfResult a, sdfResult b)
{
    return min(a.dist, b.dist);
}

float sdfSubtraction(sdfResult a, sdfResult b)
{
    return max(-a.dist, b.dist);
}

float sdfIntersection(sdfResult a, sdfResult b)
{
    return max(a.dist, b.dist);
}

float sdfSmoothUnion(sdfResult a, sdfResult b, float k)
{
    k *= 4.0;
    float h = max(k-abs(a.dist-b.dist),0.0);
    return min(a.dist, b.dist) - h*h*0.25/k;
}

float sdfSmoothSubtraction(sdfResult a, sdfResult b, float k)
{
    // return -opSmoothUnion(a, -b, k);
    k *= 4.0;
    float h = max(k-abs(-a.dist-b.dist),0.0);
    return max(-a.dist, b.dist) + h*h*0.25/k;
}
