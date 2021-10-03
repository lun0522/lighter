//
//  file.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/file.h"

#include <exception>
#include <fstream>

#include "lighter/common/util.h"
#include "lighter/shader_compiler/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_replace.h"
#include "third_party/absl/strings/str_split.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace lighter::common {
namespace {

namespace stdfs = std::filesystem;

using bazel::tools::cpp::runfiles::Runfiles;

// Used to lookup the full path of a runfile.
class RunfileLookup {
 public:
  // Initializes 'runfiles'. This only needs to be called once.
  static void Init(std::string_view arg0) {
    std::string error;
    runfiles_ = Runfiles::Create(std::string(arg0), &error);
    ASSERT_NON_NULL(runfiles_,
                    absl::StrFormat("Failed to init runfiles: %s", error));
  }

  // Returns the full path of a runfile.
  static stdfs::path GetFullPath(std::string_view relative_path) {
    ASSERT_NON_NULL(runfiles_, "EnableRunfileLookup() must be called first");
    // Bazel runfile lookup library expects forward-slash only.
    const std::string patched_relative_path =
        absl::StrReplaceAll(relative_path, {{"\\", "/"}});
    stdfs::path full_path{runfiles_->Rlocation(patched_relative_path)};
    ASSERT_TRUE(stdfs::exists(full_path),
                absl::StrFormat("Runfile '%s' does not exist", relative_path));
    return full_path;
  }

 private:
  static const Runfiles* runfiles_;
};

const Runfiles* RunfileLookup::runfiles_ = nullptr;

// Opens the file in the given `path` and checks whether it is successful.
std::ifstream OpenFile(std::string_view path) {
  // On Windows, character 26 (Ctrl+Z) is treated as EOF, so we have to include
  // std::ios::binary.
  std::ifstream file{path.data(), std::ios::in | std::ios::binary};
  ASSERT_TRUE(file, absl::StrFormat("Failed to open file '%s'", path));
  return file;
}

// Splits the given `text` by `delimiter`, while `num_segments` is the expected
// length of results. An exception will be thrown if the length does not match.
std::vector<std::string> SplitText(std::string_view text, char delimiter,
                                   int num_segments) {
  const std::vector<std::string> segments =
      absl::StrSplit(text, delimiter, absl::SkipWhitespace{});
  ASSERT_TRUE(
      segments.size() == num_segments,
      absl::StrFormat("Invalid number of segments (expected %d, but get %d)",
                      num_segments, segments.size()));
  return segments;
}

}  // namespace

namespace file {

void EnableRunfileLookup(std::string_view arg0) {
  RunfileLookup::Init(arg0);
}

std::string GetResourcePath(std::string_view relative_file_path,
                            bool want_directory_path) {
  const stdfs::path relative_path =
      stdfs::path{"resource"} / relative_file_path;
  stdfs::path full_path = RunfileLookup::GetFullPath(relative_path.string());
  if (want_directory_path) {
    full_path = full_path.parent_path();
  }
  return full_path.string();
}

std::string GetShaderBinaryPath(std::string_view relative_shader_path,
                                api::GraphicsApi graphics_api) {
  stdfs::path relative_path{"lighter/lighter/shader"};
  relative_path /= shader_compiler::util::GetShaderBinaryPath(
      graphics_api, relative_shader_path);
  return RunfileLookup::GetFullPath(relative_path.string()).string();
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
  ASSERT_TRUE(file, absl::StrFormat("Failed to read file '%s'", path));
  return data;
}

}  // namespace file

ObjFile::ObjFile(std::string_view path, int index_base) {
  std::ifstream file = OpenFile(path);

  std::vector<glm::vec3> positions;
  std::vector<glm::vec3> normals;
  std::vector<glm::vec2> tex_coords;
  absl::flat_hash_map<std::string, uint32_t> loaded_vertices;

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
            FATAL(absl::StrFormat("Unexpected symbol '%c'",
                                  line[non_space + 1]));
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
        FATAL(absl::StrFormat("Unexpected symbol '%c'", line[non_space]));
    }
  };

  std::string line;
  int line_num = 1;
  try {
    for (; std::getline(file, line); ++line_num) {
      parse_line(line);
    }
  } catch (const std::exception& e) {
    FATAL(absl::StrFormat("Failed to parse line %d: %s\n%s",
                          line_num, line, e.what()));
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

}  // namespace lighter::common
