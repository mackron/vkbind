#version 450

// Vertex Input
layout(location = 0) in vec3 VERT_Position;
layout(location = 1) in vec3 VERT_Color;

// Vertex Output / Fragment Input
layout(location = 0) out vec3 FRAG_Color;

void main()
{
    gl_Position = vec4(VERT_Position, 1);
    FRAG_Color = VERT_Color;
}