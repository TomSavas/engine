#version 450
#extension GL_EXT_buffer_reference : require

layout(set = 0, binding = 0) uniform SceneUniforms 
{
    mat4 view;
    mat4 proj;
} scene;

layout(set = 1, binding = 0) uniform sampler2D textures[]; 

struct Vertex 
{
	vec4 position;
	vec4 uv;
	vec4 normal;
	vec4 tangent;
}; 

layout(buffer_reference, std430) readonly buffer VertexBuffer
{ 
	Vertex vertices[];
};

struct ModelData 
{
	vec4 textures;
	mat4 model;
}; 

layout(buffer_reference, std430) readonly buffer ModelDataBuffer
{ 
	ModelData modelData[];
};

layout(push_constant) uniform Constants
{	
    mat4 model;
    vec4 color;
	VertexBuffer vertexBuffer;
	ModelDataBuffer modelData;
} constants;


layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;



layout (location = 1) in vec4 vertColor[];
layout (location = 5) out flat int index;

layout (location = 2) out vec3 normal;
layout (location = 3) out vec4 color;
layout (location = 4) out vec2 uv;

void main() 
{	
	vec4 a = (gl_in[0].gl_Position - gl_in[1].gl_Position);
	// a.xyz = a.xyz / a.w;
	vec4 b = (gl_in[2].gl_Position - gl_in[1].gl_Position);
	// b.xyz = b.xyz / b.w;

	// vec3 a = (gl_in[1].gl_Position.xyz / gl_in[1].gl_Position.w) - (gl_in[0].gl_Position.xyz / gl_in[0].gl_Position.w);
	// vec3 b = (gl_in[2].gl_Position.xyz / gl_in[2].gl_Position.w) - (gl_in[0].gl_Position.xyz / gl_in[0].gl_Position.w);
	// vec4 b = gl_in[2].gl_Position - gl_in[0].gl_Position;

	normal = normalize(cross(a.xyz, b.xyz));

	mat4 viewProj = scene.proj * scene.view;

	color = vertColor[0];
	gl_Position = viewProj * gl_in[0].gl_Position;
	// gl_Position = gl_in[0].gl_Position;
	EmitVertex();
	color = vertColor[1];
	gl_Position = viewProj * gl_in[1].gl_Position;
	// gl_Position = gl_in[1].gl_Position;
	EmitVertex();
	color = vertColor[2];
 	gl_Position = viewProj * gl_in[2].gl_Position;
 	// gl_Position = gl_in[2].gl_Position;
	EmitVertex();
	EndPrimitive();

	normal = normalize(cross(a.xyz, b.xyz)).xyz;
	// normal = a;

	// color = vec4(1.0, 0.0, 0.0, 1.0);
	// gl_Position = gl_in[0].gl_Position;
	// EmitVertex();
	// color = vec4(1.0, 0.0, 0.0, 1.0);
	// gl_Position = gl_in[1].gl_Position;
	// EmitVertex();
	// color = vec4(1.0, 0.0, 0.0, 1.0);
	// gl_Position = gl_in[2].gl_Position;
	// EmitVertex();
	// EndPrimitive();
 }
