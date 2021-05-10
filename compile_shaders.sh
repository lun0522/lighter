#!/bin/sh
#
# Compile shaders for all targeted graphics APIs.

# Stop if error occurs.
set -e

# On Windows, avoid to use system binaries.
case "$(uname)" in
  CYGWIN* | MINGW* ) PATH=/usr/bin:$PATH;;
esac

CURRENT_DIR=$(pwd)
SHADERS_DIR="$(dirname ${BASH_SOURCE[0]})/lighter/shader"
COMPILER_BIN="/tmp/glslangValidator"

#######################################
# Return a string containing the
# current time.
#######################################
get_time_string() {
  /bin/date "+%F %T"
}

#######################################
# Compile a shader file.
# Arguments:
#   Path to shader file.
#   Flags to use for compilation.
#   Output directory.
#######################################
compile_shader() {
  output="$3/$1.spv"
  mkdir -p "$(dirname "${output}")"
  ${COMPILER_BIN} -o "${output}" -V "$2" "$1"
}

# Download the shader compiler.
if [ ! -e ${COMPILER_BIN} ]; then
  platform="unknown"
  case "$(uname)" in
    Darwin*          ) platform="osx";;
    Linux*           ) platform="linux";;
    CYGWIN* | MINGW* ) platform="windows-x64";;
    *                ) echo "Unsupported platform"; exit 1;;
  esac
  compiler_addr="https://github.com/KhronosGroup/glslang/releases/download/master-tot/glslang-master-${platform}-Release.zip"

  echo "$(get_time_string) Downloading shader compiler..."
  compressed_file="/tmp/glslang.zip"
  curl -L ${compiler_addr} > ${compressed_file}
  unzip -p ${compressed_file} bin/glslangValidator* > ${COMPILER_BIN}
  chmod 700 ${COMPILER_BIN}
  rm ${compressed_file}
fi

# Compile shaders.
echo "$(get_time_string) Compiling shaders..."
cd "${SHADERS_DIR}"
for suffix in "*.vert" "*.frag" "*.comp"; do
  find . -type f -name "${suffix}" | while read -r file; do
    compile_shader "${file}" -DTARGET_OPENGL "opengl"
    compile_shader "${file}" -DTARGET_VULKAN "vulkan"
  done
done

cd "${CURRENT_DIR}"
echo "$(get_time_string) Done!"
