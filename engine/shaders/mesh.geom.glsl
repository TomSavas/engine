#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters : require
#extension GL_ARB_shading_language_include : require

#include "mesh.glsl"

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

layout (location = 0) in MESH_VS_OUT gsIn[];

layout (location = 10) out MESH_VS_OUT gsOut;
layout (location = 21) out vec3 barycentric;

void main()
{
    const vec3 barycentricVertex[3] = {
        vec3(1.f, 0.f, 0.f),
        vec3(0.f, 1.f, 0.f),
        vec3(0.f, 0.f, 1.f),
    };

    for (int i = 0; i < 3; i++)
    {
        gl_Position = gl_in[i].gl_Position;
        barycentric = barycentricVertex[i];

#define COPY_COMPONENT(M, T, X) gsOut.X = gsIn[i].X
        MESH_VS_OUT_COMPONENT(COPY_COMPONENT)

        EmitVertex();
    }

    EndPrimitive();
}
