#!/bin/sh

set -e

BASEDIR=$(dirname "$0")
COMPILER="${BASEDIR}/third_party/glslangValidator"
SHADERS_DIR="${BASEDIR}/jessie_steamer/shader"
COMPILED_GL_DIR="${SHADERS_DIR}/opengl"
COMPILED_VK_DIR="${SHADERS_DIR}/vulkan"
COMPILED_EXT=".spv"

echo "Compiling shaders..."
for ext in ".vert" ".frag"; do
  for file in ${SHADERS_DIR}/*${ext}; do
    output=$(basename ${file})${COMPILED_EXT}
    ${COMPILER} -V -DTARGET_OPENGL ${file} -o "${COMPILED_GL_DIR}/${output}"
    ${COMPILER} -V -DTARGET_VULKAN ${file} -o "${COMPILED_VK_DIR}/${output}"
  done
done
echo "Finished!"
