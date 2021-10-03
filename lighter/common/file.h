//
//  file.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_FILE_H
#define LIGHTER_COMMON_FILE_H

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/common/data.h"
#include "lighter/common/graphics_api.h"
#include "third_party/glm/glm.hpp"

namespace lighter::common {
namespace file {

// Enables looking up the runtime path of runfiles (i.e. data dependencies of
// Bazel-built binaries and tests). This should be called once with argv[0] in
// main() before accessing any runfiles.
void EnableRunfileLookup(std::string_view arg0);

// Returns the full path to a file or directory in the resource folder.
// Since Bazel only maintains a manifest for file path, in order to get a
// directory path, we should pass in the path to any file in that directory, and
// set `want_directory_path` to true.
std::string GetResourcePath(std::string_view relative_file_path,
                            bool want_directory_path = false);

// Returns the full path to the shader binary.
std::string GetShaderBinaryPath(std::string_view relative_shader_path,
                                api::GraphicsApi graphics_api);

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

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_FILE_H
