#version 450

// Descriptor sets. This example uses separate textures and samplers, but you can also use a combined
// texture/sampler if that suits your scenario better.
layout(set = 0, binding = 0) uniform sampler   FRAG_Sampler;
layout(set = 0, binding = 1) uniform texture2D FRAG_Texture;


// Fragment Input
layout(location = 0) in vec3 FRAG_Color;
layout(location = 1) in vec2 FRAG_TexCoord;

// Fragment Output
layout(location = 0) out vec4 OUT_Color;

void main()
{
    OUT_Color = vec4(FRAG_Color, 1) * texture(sampler2D(FRAG_Texture, FRAG_Sampler), FRAG_TexCoord);
}