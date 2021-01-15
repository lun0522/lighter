//
//  file.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/file.h"

#include <fstream>

#include "lighter/common/util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/strings/str_split.h"

ABSL_FLAG(std::string, resource_folder, "external/resource",
          "Path to the resource folder");
ABSL_FLAG(std::string, shader_folder, "lighter/shader",
          "Path to the shader folder");

#if defined(__APPLE__)
#define VULKAN_FOLDER "external/lib-vulkan-osx"
#elif defined(__linux__)
#define VULKAN_FOLDER "external/lib-vulkan-linux"
#endif  // __APPLE__ || __linux__

ABSL_FLAG(std::string, vulkan_folder, VULKAN_FOLDER,
          "Path to the Vulkan SDK folder");

#undef VULKAN_FOLDER

namespace lighter::common {
namespace {

// Opens the file in the given 'path' and checks whether it is successful.
std::ifstream OpenFile(std::string_view path) {
  std::ifstream file{path.data()};
  ASSERT_FALSE(!file.is_open() || file.bad() || file.fail(),
               absl::StrCat("Failed to open file: ", path));
  return file;
}

// Splits the given 'text' by 'delimiter', while 'num_segments' is the expected
// length of results. An exception will be thrown if the length does not match.
std::vector<std::string> SplitText(std::string_view text, char delimiter,
                                   int num_segments) {
  const std::vector<std::string> result =
      absl::StrSplit(text, delimiter, absl::SkipWhitespace{});
  ASSERT_TRUE(
      result.size() == num_segments,
      absl::StrFormat("Invalid number of segments (expected %d, but get %d)",
                      num_segments, result.size()));
  return result;
}

}  // namespace

RawData::RawData(std::string_view path) {
  std::ifstream file = OpenFile(path);
  file.seekg(0, std::ios::end);
  size = file.tellg();
  auto* content = new char[size];
  file.seekg(0, std::ios::beg);
  file.read(content, size);
  data = content;
}

#define APPEND_ATTRIBUTES(attributes, type, member) \
    file::AppendVertexAttributes<decltype(type::member)>( \
        attributes, offsetof(type, member))

std::vector<VertexAttribute> Vertex2DPosOnly::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex2DPosOnly, pos);
  return attributes;
}

std::vector<VertexAttribute> Vertex2D::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex2D, pos);
  APPEND_ATTRIBUTES(attributes, Vertex2D, tex_coord);
  return attributes;
}

std::vector<VertexAttribute> Vertex3DPosOnly::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex3DPosOnly, pos);
  return attributes;
}

std::vector<VertexAttribute> Vertex3DWithColor::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex3DWithColor, pos);
  APPEND_ATTRIBUTES(attributes, Vertex3DWithColor, color);
  return attributes;
}

std::vector<VertexAttribute> Vertex3DWithTex::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex3DWithTex, pos);
  APPEND_ATTRIBUTES(attributes, Vertex3DWithTex, norm);
  APPEND_ATTRIBUTES(attributes, Vertex3DWithTex, tex_coord);
  return attributes;
}

#undef APPEND_ATTRIBUTES

namespace file {

template <>
void AppendVertexAttributes<glm::mat4>(std::vector<VertexAttribute>& attributes,
                                       int offset_bytes) {
  attributes.reserve(attributes.size() + glm::mat4::length());
  for (int i = 0; i < glm::mat4::length(); ++i) {
    AppendVertexAttributes<glm::vec4>(attributes, offset_bytes);
    offset_bytes += sizeof(glm::vec4);
  }
}

}  // namespace file

std::array<Vertex2DPosOnly, 6> Vertex2DPosOnly::GetFullScreenSquadVertices() {
  return {
      Vertex2DPosOnly{.pos = {-1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = { 1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = { 1.0f,  1.0f}},
      Vertex2DPosOnly{.pos = {-1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = { 1.0f,  1.0f}},
      Vertex2DPosOnly{.pos = {-1.0f,  1.0f}},
  };
}

std::array<Vertex2D, 6> Vertex2D::GetFullScreenSquadVertices(bool flip_y) {
  if (flip_y) {
    return {
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 1.0f}},
        Vertex2D{.pos = { 1.0f, -1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 1.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {-1.0f,  1.0f}, .tex_coord = {0.0f, 0.0f}},
    };
  } else {
    return {
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 0.0f}},
        Vertex2D{.pos = { 1.0f, -1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 0.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {-1.0f,  1.0f}, .tex_coord = {0.0f, 1.0f}},
    };
  }
}

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
