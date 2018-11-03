glslangValidator -V ..\..\examples\shaders\01_Triangle.glsl.vert -o 01_Triangle.vert.spirv
dred -f file2chex 01_Triangle.vert.spirv -n g_VertexShaderData > 01_Triangle.vert.c

glslangValidator -V ..\..\examples\shaders\01_Triangle.glsl.frag -o 01_Triangle.frag.spirv
dred -f file2chex 01_Triangle.frag.spirv -n g_FragmentShaderData > 01_Triangle.frag.c