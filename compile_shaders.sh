#!/bin/sh
#
# Compile shaders for all targeted graphics APIs.

set -e

COMPILER_ADDR="https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-osx-Release.zip"
COMPILER_BIN="/tmp/glslangValidator"
BASE_DIR=$(dirname "$0")
SHADERS_DIR="${BASE_DIR}/jessie_steamer/shader"
COMPILED_GL_DIR="${SHADERS_DIR}/opengl"
COMPILED_VK_DIR="${SHADERS_DIR}/vulkan"
OUTPUT_FILE_EXT=".spv"
DATE_FORMAT="+%F %T"

#######################################
# Compile a shader file.
# Arguments:
#   Path to shader file.
#   Flags to use for compilation.
#   Output directory.
#######################################
compile_shader() {
  output="$3/$1${OUTPUT_FILE_EXT}"
  mkdir -p "$(dirname "${output}")"
  ${COMPILER_BIN} -o "${output}" -V "$2" "$1"
}

# Download the shader compiler.
if [ ! -e ${COMPILER_BIN} ]; then
  echo "$(DATE "${DATE_FORMAT}") Downloading shader compiler..."
  COMPRESSED="/tmp/glslang.zip"
  wget -O ${COMPRESSED} ${COMPILER_ADDR}
  unzip -p ${COMPRESSED} bin/glslangValidator > ${COMPILER_BIN}
  chmod 700 ${COMPILER_BIN}
  rm ${COMPRESSED}
fi

# Compile shaders.
echo "$(DATE "${DATE_FORMAT}") Compiling shaders..."
cd "${SHADERS_DIR}"
for suffix in "*.vert" "*.frag"; do
  find . -type f -name "${suffix}" | while read -r file; do
    compile_shader "${file}" -DTARGET_OPENGL "${COMPILED_GL_DIR}"
    compile_shader "${file}" -DTARGET_VULKAN "${COMPILED_VK_DIR}"
  done
done
echo "$(DATE "${DATE_FORMAT}") Done!"
