#!/bin/sh

VK_GLSL_VALIDATOR="/Users/lun/Desktop/Code/libs/vulkan/macOS/bin/glslangValidator"
SHADERS_DIR="/Users/lun/Desktop/Code/LearnVulkan/jessie_engine/shader"
COMPILED_DIR="/Users/lun/Desktop/Code/LearnVulkan/jessie_engine/shader/compiled"
COMPILED_EXT=".spv"

echo "Compiling shaders..."
for ext in ".vert" ".frag"; do
  for file in ${SHADERS_DIR}/*${ext}; do
    output="${COMPILED_DIR}/$(basename ${file})${COMPILED_EXT}"
    ${VK_GLSL_VALIDATOR} -V ${file} -o ${output}
  done
done
