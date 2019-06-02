#!/bin/sh

set -e

COMPILER="/tmp/glslangValidator"
BASEDIR=$(dirname "$0")
SHADERS_DIR="${BASEDIR}/jessie_steamer/shader"
COMPILED_GL_DIR="${SHADERS_DIR}/opengl"
COMPILED_VK_DIR="${SHADERS_DIR}/vulkan"
COMPILED_EXT=".spv"

if [[ ! -e ${COMPILER} ]]; then
  echo "Downloading compiler..."
  COMPRESSED="/tmp/glslang.zip"
  wget -O ${COMPRESSED} \
    https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-osx-Release.zip
  unzip -p ${COMPRESSED} bin/glslangValidator > ${COMPILER}
  chmod 700 ${COMPILER}
  rm ${COMPRESSED}
fi

echo "Compiling shaders..."
for ext in ".vert" ".frag"; do
  for file in ${SHADERS_DIR}/*${ext}; do
    output=$(basename ${file})${COMPILED_EXT}
    ${COMPILER} -V -DTARGET_OPENGL ${file} -o "${COMPILED_GL_DIR}/${output}"
    ${COMPILER} -V -DTARGET_VULKAN ${file} -o "${COMPILED_VK_DIR}/${output}"
  done
done
echo "Finished!"
