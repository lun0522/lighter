#!/bin/sh

set -e

COMPILER_ADDR="https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-osx-Release.zip"
COMPILER_BIN="/tmp/glslangValidator"
BASE_DIR=$(dirname "$0")
SHADERS_DIR="${BASE_DIR}/jessie_steamer/shader"
COMPILED_GL_DIR="${SHADERS_DIR}/opengl"
COMPILED_VK_DIR="${SHADERS_DIR}/vulkan"
COMPILED_EXT=".spv"

if [ ! -e ${COMPILER_BIN} ]; then
  echo "Downloading shader compiler..."
  COMPRESSED="/tmp/glslang.zip"
  wget -O ${COMPRESSED} ${COMPILER_ADDR}
  unzip -p ${COMPRESSED} bin/glslangValidator > ${COMPILER_BIN}
  chmod 700 ${COMPILER_BIN}
  rm ${COMPRESSED}
fi

echo "Compiling shaders..."
for ext in ".vert" ".frag"; do
  for file in "${SHADERS_DIR}"/*"${ext}"; do
    output=$(basename "${file}")${COMPILED_EXT}
    ${COMPILER_BIN} -V -DTARGET_OPENGL "${file}" -o "${COMPILED_GL_DIR}/${output}"
    ${COMPILER_BIN} -V -DTARGET_VULKAN "${file}" -o "${COMPILED_VK_DIR}/${output}"
  done
done
echo "Finished!"
