#version 450

// Fragment Input
layout(location = 0) in vec3 FRAG_Color;

// Fragment Output
layout(location = 0) out vec4 OUT_Color;

void main()
{
    OUT_Color = vec4(FRAG_Color, 1);
}