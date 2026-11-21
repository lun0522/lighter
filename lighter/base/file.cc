//
//  file.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/base/file.h"

#include <cstdlib>
#include <exception>
#include <format>
#include <fstream>
#include <unordered_map>

#include "lighter/base/flag.h"
#include "lighter/base/util.h"

LIGHTER_FLAG(std::string, resource_dir, "",
             "Path to the directory containing runtime resource assets");
LIGHTER_FLAG(std::string, shader_dir, "",
             "Path to the directory containing compiled shader binaries");

namespace lighter::base {
namespace {

namespace stdfs = std::filesystem;

constexpr const char kSpirvBinaryFileExtension[] = ".spv";

// Resolves a base directory from a CLI flag, an environment variable, an
// optional compile-time default, or a workspace-relative fallback path.
stdfs::path ResolveAssetDir(std::string_view flag_value, const char* env_var,
                            const char* compile_time_default,
                            const stdfs::path& cwd_fallback,
                            std::string_view dir_label) {
  if (!flag_value.empty()) {
    stdfs::path path{flag_value};
    ASSERT_TRUE(stdfs::is_directory(path),
                std::format("{} '{}' is not a valid directory", dir_label,
                            path.string()));
    return path;
  }

  if (const char* env_value = std::getenv(env_var);
      env_value != nullptr && env_value[0] != '\0') {
    stdfs::path path{env_value};
    ASSERT_TRUE(stdfs::is_directory(path),
                std::format("{} '{}' (from {}) is not a valid directory",
                            dir_label, path.string(), env_var));
    return path;
  }

  if (compile_time_default != nullptr && compile_time_default[0] != '\0') {
    stdfs::path path{compile_time_default};
    if (stdfs::is_directory(path)) {
      return path;
    }
  }

  ASSERT_TRUE(stdfs::is_directory(cwd_fallback),
              std::format("Failed to locate {}; pass --{} or set {}", dir_label,
                          dir_label, env_var));
  return cwd_fallback;
}

const stdfs::path& GetResourceDir() {
#ifdef LIGHTER_RESOURCE_DIR
  constexpr const char* kDefaultResourceDir = LIGHTER_RESOURCE_DIR;
#else
  constexpr const char* kDefaultResourceDir = nullptr;
#endif
  static const auto* resource_dir = new stdfs::path{ResolveAssetDir(
      FLAGS_resource_dir.Get(), "LIGHTER_RESOURCE_DIR", kDefaultResourceDir,
      stdfs::path{"resource"}, "resource_dir")};
  return *resource_dir;
}

const stdfs::path& GetShaderDir() {
#ifdef LIGHTER_SHADER_DIR
  constexpr const char* kDefaultShaderDir = LIGHTER_SHADER_DIR;
#else
  constexpr const char* kDefaultShaderDir = nullptr;
#endif
  static const auto* shader_dir = new stdfs::path{ResolveAssetDir(
      FLAGS_shader_dir.Get(), "LIGHTER_SHADER_DIR", kDefaultShaderDir,
      stdfs::path{"lighter/shader"}, "shader_dir")};
  return *shader_dir;
}

// Opens the file in the given `path` and checks whether it is successful.
std::ifstream OpenFile(std::string_view path) {
  // On Windows, character 26 (Ctrl+Z) is treated as EOF, so we have to include
  // std::ios::binary.
  std::ifstream file{path.data(), std::ios::in | std::ios::binary};
  ASSERT_TRUE(file, std::format("Failed to open file '{}'", path));
  return file;
}

// Splits the given `text` by `delimiter`, while `num_segments` is the expected
// length of results. An exception will be thrown if the length does not match.
std::vector<std::string> SplitText(std::string_view text, char delimiter,
                                   int num_segments) {
  const std::vector<std::string> segments =
      util::StrSplit(text, delimiter, /*skip_empty=*/true);
  ASSERT_TRUE(
      segments.size() == num_segments,
      std::format("Invalid number of segments (expected {}, but get {})",
                  num_segments, segments.size()));
  return segments;
}

}  // namespace

namespace file {

std::string GetResourcePath(std::string_view relative_file_path,
                            bool want_directory_path) {
  stdfs::path full_path = GetResourceDir() / relative_file_path;
  ASSERT_TRUE(stdfs::exists(full_path),
              std::format("Resource '{}' does not exist", full_path.string()));
  if (want_directory_path) {
    full_path = full_path.parent_path();
  }
  return full_path.string();
}

std::string GetShaderBinaryPath(std::string_view relative_shader_path) {
  stdfs::path full_path = GetShaderDir() / relative_shader_path;
  full_path += kSpirvBinaryFileExtension;
  ASSERT_TRUE(
      stdfs::exists(full_path),
      std::format("Shader binary '{}' does not exist", full_path.string()));
  return full_path.string();
}

std::string GetVulkanSdkPath(std::string_view relative_path) {
  static const stdfs::path* vk_sdk_path = nullptr;
  if (vk_sdk_path == nullptr) {
    const char* env_var = getenv("VULKAN_SDK");
    ASSERT_NON_NULL(env_var, "Environment variable 'VULKAN_SDK' not set");
    vk_sdk_path = new stdfs::path{env_var};
  }
  return (*vk_sdk_path / relative_path).string();
}

Data LoadDataFromFile(std::string_view path) {
  std::ifstream file = OpenFile(path);
  file.seekg(0, std::ios::end);
  const size_t size = file.tellg();
  Data data{size};
  file.seekg(0, std::ios::beg);
  file.read(data.mut_data<char>(), size);
  ASSERT_TRUE(file, std::format("Failed to read file '{}'", path));
  return data;
}

}  // namespace file

ObjFile::ObjFile(std::string_view path, int index_base) {
  std::ifstream file = OpenFile(path);

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> tex_coords;
  std::unordered_map<std::string, uint32_t> loaded_vertices;

  const auto parse_line = [&](std::string_view line) {
    const size_t non_space = line.find_first_not_of(' ');
    if (non_space == std::string::npos || line[0] == '#') {
      // Skip blank lines and comments.
      return;
    }

    switch (line[non_space]) {
      case 'v': {
        // Either position, normal or texture coordinates.
        switch (line[non_space + 1]) {
          case ' ': {
            // Position.
            const auto nums = SplitText(line.substr(non_space + 2), ' ',
                                        /*num_segments=*/3);
            positions.push_back(
                glm::vec3{stof(nums[0]), stof(nums[1]), stof(nums[2])});
            break;
          }
          case 'n': {
            // Normal.
            const auto nums = SplitText(line.substr(non_space + 3), ' ',
                                        /*num_segments=*/3);
            normals.push_back(
                glm::vec3{stof(nums[0]), stof(nums[1]), stof(nums[2])});
            break;
          }
          case 't': {
            // Texture coordinates.
            const auto nums = SplitText(line.substr(non_space + 3), ' ',
                                        /*num_segments=*/2);
            tex_coords.push_back(glm::vec2{stof(nums[0]), stof(nums[1])});
            break;
          }
          default:
            FATAL(std::format("Unexpected symbol '{}'", line[non_space + 1]));
        }
        break;
      }
      case 'f': {
        // Face.
        for (const auto& seg : SplitText(line.substr(non_space + 2), ' ',
                                         /*num_segments=*/3)) {
          const auto iter = loaded_vertices.find(seg);
          if (iter != loaded_vertices.end()) {
            indices.push_back(iter->second);
          } else {
            indices.push_back(vertices.size());
            loaded_vertices[seg] = vertices.size();
            const auto idxs = SplitText(seg, '/', /*num_segments=*/3);
            vertices.push_back(Vertex3DWithTex{
                positions.at(stoi(idxs[0]) - index_base),
                normals.at(stoi(idxs[2]) - index_base),
                tex_coords.at(stoi(idxs[1]) - index_base),
            });
          }
        }
        break;
      }
      default:
        FATAL(std::format("Unexpected symbol '{}'", line[non_space]));
    }
  };

  std::string line;
  int line_num = 1;
  try {
    for (; std::getline(file, line); ++line_num) {
      parse_line(line);
    }
  } catch (const std::exception& e) {
    FATAL(std::format("Failed to parse line {}: {}\n{}", line_num, line,
                      e.what()));
  }
}

ObjFilePosOnly::ObjFilePosOnly(std::string_view path, int index_base) {
  ObjFile file(path, index_base);
  indices = std::move(file.indices);
  vertices.reserve(file.vertices.size());
  for (const auto& vertex : file.vertices) {
    vertices.push_back({vertex.pos});
  }
}

}  // namespace lighter::base
