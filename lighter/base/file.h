#ifndef LIGHTER_BASE_FILE_H
#define LIGHTER_BASE_FILE_H

#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/base/data.h"

namespace lighter::base {
namespace file {

// Returns the full path to a file or directory in the resource folder.
// If `want_directory_path` is true, returns the parent directory of
// `relative_file_path`.
std::string GetResourcePath(std::string_view relative_file_path,
                            bool want_directory_path = false);

// Returns the full path to the shader binary.
std::string GetShaderBinaryPath(std::string_view relative_shader_path);

// Returns the full path to files in the Vulkan SDK folder.
std::string GetVulkanSdkPath(std::string_view relative_path);

// Reads the data from file in `path`.
Data LoadDataFromFile(std::string_view path);

}  // namespace file

// Loads Wavefront .obj file.
struct ObjFile {
  ObjFile(std::string_view path, int index_base);

  // This class is neither copyable nor movable.
  ObjFile(const ObjFile&) = delete;
  ObjFile& operator=(const ObjFile&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<uint32_t> indices;
  std::vector<Vertex3DWithTex> vertices;
};

// Loads Wavefront .obj file but only preserves vertex positions.
struct ObjFilePosOnly {
  ObjFilePosOnly(std::string_view path, int index_base);

  // This class is neither copyable nor movable.
  ObjFilePosOnly(const ObjFilePosOnly&) = delete;
  ObjFilePosOnly& operator=(const ObjFilePosOnly&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<uint32_t> indices;
  std::vector<Vertex3DPosOnly> vertices;
};

}  // namespace lighter::base

#endif  // LIGHTER_BASE_FILE_H
