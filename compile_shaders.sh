#!/bin/sh
#
# Compile shaders for all targeted graphics APIs.

# Stop if error occurs.
set -e

COMPILER_BIN="/tmp/glslangValidator"
BASE_DIR=$(dirname "$0")
SHADERS_DIR="${BASE_DIR}/lighter/shader"
OUTPUT_FILE_EXT=".spv"
DATE_BIN="/bin/date"
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
  platform="unknown"
  case ${OSTYPE} in
    darwin* ) platform="osx";;
    linux*  ) platform="linux";;
    *       ) echo "Unsupported platform"; exit 1;;
  esac
  compiler_addr="https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-${platform}-Release.zip"

  echo "$(${DATE_BIN} "${DATE_FORMAT}") Downloading shader compiler..."
  COMPRESSED="/tmp/glslang.zip"
  curl -L -o ${COMPRESSED} ${compiler_addr}
  unzip -p ${COMPRESSED} bin/glslangValidator > ${COMPILER_BIN}
  chmod 700 ${COMPILER_BIN}
  rm ${COMPRESSED}
fi

# Compile shaders.
echo "$(${DATE_BIN} "${DATE_FORMAT}") Compiling shaders..."
cd "${SHADERS_DIR}"
for suffix in "*.vert" "*.frag" "*.comp"; do
  find . -type f -name "${suffix}" | while read -r file; do
    compile_shader "${file}" -DTARGET_OPENGL "opengl"
    compile_shader "${file}" -DTARGET_VULKAN "vulkan"
  done
done
echo "$(${DATE_BIN} "${DATE_FORMAT}") Done!"
