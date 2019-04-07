#!/bin/sh

VK_GLSL_VALIDATOR=$1
BASEDIR=$(dirname "$0")
SHADERS_DIR="${BASEDIR}/jessie_steamer/shader"
COMPILED_DIR="${SHADERS_DIR}/compiled"
COMPILED_EXT=".spv"

echo "Compiling shaders..."
for ext in ".vert" ".frag"; do
  for file in ${SHADERS_DIR}/*${ext}; do
    output="${COMPILED_DIR}/$(basename ${file})${COMPILED_EXT}"
    ${VK_GLSL_VALIDATOR} -V ${file} -o ${output}
  done
done
echo "Finished!"
