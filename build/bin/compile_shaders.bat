glslangValidator -V ..\..\examples\shaders\01_Triangle\01_Triangle.glsl.vert -o ..\..\examples\resources\01_Triangle\01_Triangle.vert.spirv
dred -f file2chex ..\..\examples\resources\01_Triangle\01_Triangle.vert.spirv -n g_VertexShaderData > ..\..\examples\resources\01_Triangle\01_Triangle.vert.c

glslangValidator -V ..\..\examples\shaders\01_Triangle\01_Triangle.glsl.frag -o ..\..\examples\resources\01_Triangle\01_Triangle.frag.spirv
dred -f file2chex ..\..\examples\resources\01_Triangle\01_Triangle.frag.spirv -n g_FragmentShaderData > ..\..\examples\resources\01_Triangle\01_Triangle.frag.c