//
//  file.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/file.h"

#include <fstream>
#include <stdexcept>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

namespace jessie_steamer {
namespace common {
namespace {

using std::ifstream;
using std::runtime_error;
using std::stof;
using std::string;
using std::vector;

ifstream OpenFile(const string& path) {
  ifstream file{path};
  if (!file.is_open() || file.bad() || file.fail()) {
    throw runtime_error{"Failed to open file: " + path};
  }
  return file;
}

inline absl::string_view GetSuffix(const string& text,
                                   size_t start_pos) {
  return absl::string_view{text.c_str() + start_pos, text.length() - start_pos};
}

vector<string> SplitText(absl::string_view text,
                         char delimiter,
                         int num_segment) {
  vector<string> result = absl::StrSplit(text, delimiter);
  if (result.size() != num_segment) {
    throw runtime_error{absl::StrFormat(
        "Wrong number of segments (expected %d, but get %d)",
        num_segment, result.size())};
  }
  return result;
}

} /* namespace */

RawData::RawData(const string& path) {
  ifstream file = OpenFile(path);
  file.seekg(0, std::ios::end);
  size = file.tellg();
  auto* content = new char[size];
  file.seekg(0, std::ios::beg);
  file.read(content, size);
  data = content;
}

Image::Image(const string& path) {
  const auto raw_data = absl::make_unique<RawData>(path);
  data = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(raw_data->data),
                               static_cast<int>(raw_data->size),
                               &width, &height, &channel, STBI_default);
  if (data == nullptr) {
    throw runtime_error{"Failed to read image from " + path};
  }

  switch (channel) {
    case 1:
    case 4:
      break;
    case 3: {  // force to have alpha channel
      stbi_image_free(const_cast<void*>(data));
      data = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(raw_data->data),
          static_cast<int>(raw_data->size),
          &width, &height, &channel, STBI_rgb_alpha);
      channel = STBI_rgb_alpha;
      break;
    }
    default:
      throw runtime_error{absl::StrFormat(
          "Unsupported number of channels: %d", channel)};
  }
}

Image::~Image() {
  stbi_image_free(const_cast<void*>(data));
}

namespace file {

void LoadObjFile(const string& path,
                 unsigned int index_base,
                 vector<VertexAttrib3D>* vertices,
                 vector<uint32_t>* indices) {
  ifstream file = OpenFile(path);

  vector<glm::vec3> positions;
  vector<glm::vec3> normals;
  vector<glm::vec2> tex_coords;
  absl::flat_hash_map<string, uint32_t> loaded_vertices;

  auto parse_line = [&](const string& line) {
    size_t non_space = line.find_first_not_of(' ');
    if (non_space == string::npos || line[0] == '#') {
      return;  // skip blank lines and comments
    }

    switch (line[non_space]) {
      case 'v': {  // pos, norm or tex_coord
        switch (line[non_space + 1]) {
          case ' ': {  // pos
            const auto nums = SplitText(GetSuffix(line, non_space + 2), ' ',
                                        /*num_segment=*/3);
            positions.emplace_back(stof(nums[0]), stof(nums[1]), stof(nums[2]));
            break;
          }
          case 'n': {  // norm
            auto nums = SplitText(GetSuffix(line, non_space + 3), ' ',
                                  /*num_segment=*/3);
            normals.emplace_back(stof(nums[0]), stof(nums[1]), stof(nums[2]));
            break;
          }
          case 't': {  // tex_coord
            auto nums = SplitText(GetSuffix(line, non_space + 3), ' ',
                                  /*num_segment=*/2);
            tex_coords.emplace_back(stof(nums[0]), stof(nums[1]));
            break;
          }
          default:
            throw runtime_error{absl::StrFormat(
                "Unexpected symbol '%c'", line[non_space + 1])};
        }
        break;
      }
      case 'f': {  // face
        for (const auto& seg : SplitText(GetSuffix(line, non_space + 2), ' ',
                                         /*num_segment=*/3)) {
          auto found = loaded_vertices.find(seg);
          if (found != loaded_vertices.end()) {
            indices->emplace_back(found->second);
          } else {
            indices->emplace_back(vertices->size());
            loaded_vertices.emplace(seg, vertices->size());
            auto idxs = SplitText(seg, '/', /*num_segment=*/3);
            vertices->emplace_back(
                positions.at(stoi(idxs[0]) - index_base),
                normals.at(stoi(idxs[2]) - index_base),
                tex_coords.at(stoi(idxs[1]) - index_base)
            );
          }
        }
        break;
      }
      default:
        throw runtime_error{absl::StrFormat(
            "Unexpected symbol '%c'", line[non_space])};
    }
  };

  int line_num = 1;
  for (string line; getline(file, line); ++line_num) {
    try {
      parse_line(line);
    } catch (const std::out_of_range& e) {
      throw runtime_error{absl::StrFormat(
          "Out of range at line %d: %s", line_num, line)};
    } catch (const std::invalid_argument& e) {
      throw runtime_error{absl::StrFormat(
          "Invalid argument at line %d: %s", line_num, line)};
    } catch (const std::exception& e) {
      throw runtime_error{absl::StrFormat(
          "Failed to parse line %d: %s", line_num, line)};
    }
  }
}

} /* namespace file */
} /* namespace common */
} /* namespace jessie_steamer */
