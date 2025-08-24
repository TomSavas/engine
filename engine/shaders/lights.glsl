struct LightTile
{
    uint count;
    uint offset;
};

layout (buffer_reference, std430) buffer LightTileData
{
    LightTile tiles[];
};

layout (buffer_reference, std430) buffer LightIds
{
    //uint lightIdCount;
    uint ids[];
};

layout (buffer_reference, std430) buffer LightIdCount
{
    uint lightIdCount;
};

struct PointLight
{
    vec4 pos;
    vec4 color;
    vec4 range;
};

layout (buffer_reference, std430) buffer Lights
{
    uint pointLightCount;
    PointLight pointLights[];
};