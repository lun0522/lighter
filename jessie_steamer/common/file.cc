//
//  file.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/file.h"

#include <cstdlib>
#include <fstream>

#include "absl/container/flat_hash_map.h"
#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "jessie_steamer/common/util.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

namespace jessie_steamer {
namespace common {
namespace {

using std::ifstream;
using std::stof;
using std::string;
using std::vector;

// Opens the file in the given 'path' and checks whether it is successful.
ifstream OpenFile(const string& path) {
  ifstream file{path};
  if (!file.is_open() || file.bad() || file.fail()) {
    FATAL("Failed to open file: " + path);
  }
  return file;
}

// Returns the suffix of the given 'text', starting from index 'start_pos'.
inline absl::string_view GetSuffix(const string& text, size_t start_pos) {
  return absl::string_view{text.c_str() + start_pos, text.length() - start_pos};
}

// Splits the given 'text' by 'delimiter', while 'num_segment' is the expected
// length of results. An exception will be thrown if the length does not match.
vector<string> SplitText(absl::string_view text, char delimiter,
                         int num_segment) {
  vector<string> result = absl::StrSplit(text, delimiter);
  if (result.size() != num_segment) {
    FATAL(absl::StrFormat(
        "Invalid number of segments (expected %d, but get %d)",
        num_segment, result.size()));
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
    FATAL("Failed to read image from " + path);
  }

  switch (channel) {
    case 1:
    case 4:
      break;
    case 3: {
      stbi_image_free(const_cast<void*>(data));
      data = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(raw_data->data),
          static_cast<int>(raw_data->size),
          &width, &height, &channel, STBI_rgb_alpha);
      channel = STBI_rgb_alpha;
      break;
    }
    default:
      FATAL(absl::StrFormat("Unsupported number of channels: %d", channel));
  }
}

Image::Image(int width, int height, int channel,
             const void* raw_data, bool flip_y)
    : width{width}, height{height}, channel{channel} {
  if (channel != 1 && channel != 4) {
    FATAL(absl::StrFormat("Unsupported number of channels: %d", channel));
  }

  size_t total_size = width * height * channel;
  data = std::malloc(total_size);
  if (flip_y) {
    const int stride = width * channel;
    for (int row = 0; row < height; ++row) {
      std::memcpy(
          static_cast<char*>(const_cast<void*>(data)) + stride * row,
          static_cast<const char*>(raw_data) + stride * (height - row - 1),
          stride);
    }
  } else {
    std::memcpy(const_cast<void*>(data), raw_data, total_size);
  }
}

Image::~Image() {
  std::free(const_cast<void*>(data));
}

ObjFile::ObjFile(const string& path, int index_base) {
  ifstream file = OpenFile(path);

  vector<glm::vec3> positions;
  vector<glm::vec3> normals;
  vector<glm::vec2> tex_coords;
  absl::flat_hash_map<string, uint32_t> loaded_vertices;

  auto parse_line = [&](const string& line) {
    size_t non_space = line.find_first_not_of(' ');
    if (non_space == string::npos || line[0] == '#') {
      // Skip blank lines and comments.
      return;
    }

    switch (line[non_space]) {
      case 'v': {
        // Either position, normal or texture coordinates.
        switch (line[non_space + 1]) {
          case ' ': {
            // Position.
            const auto nums = SplitText(GetSuffix(line, non_space + 2), ' ',
                                        /*num_segment=*/3);
            positions.emplace_back(stof(nums[0]), stof(nums[1]), stof(nums[2]));
            break;
          }
          case 'n': {
            // Normal.
            auto nums = SplitText(GetSuffix(line, non_space + 3), ' ',
                                  /*num_segment=*/3);
            normals.emplace_back(stof(nums[0]), stof(nums[1]), stof(nums[2]));
            break;
          }
          case 't': {
            // Texture coordinates.
            auto nums = SplitText(GetSuffix(line, non_space + 3), ' ',
                                  /*num_segment=*/2);
            tex_coords.emplace_back(stof(nums[0]), stof(nums[1]));
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
        for (const auto& seg : SplitText(GetSuffix(line, non_space + 2), ' ',
                                         /*num_segment=*/3)) {
          auto found = loaded_vertices.find(seg);
          if (found != loaded_vertices.end()) {
            indices.emplace_back(found->second);
          } else {
            indices.emplace_back(vertices.size());
            loaded_vertices.emplace(seg, vertices.size());
            auto idxs = SplitText(seg, '/', /*num_segment=*/3);
            vertices.emplace_back(
                positions.at(stoi(idxs[0]) - index_base),
                normals.at(stoi(idxs[2]) - index_base),
                tex_coords.at(stoi(idxs[1]) - index_base)
            );
          }
        }
        break;
      }
      default:
        FATAL(absl::StrFormat("Unexpected symbol '%c'", line[non_space]));
    }
  };

  int line_num = 1;
  for (string line; getline(file, line); ++line_num) {
    try {
      parse_line(line);
    } catch (const std::out_of_range& e) {
      FATAL(absl::StrFormat("Out of range at line %d: %s", line_num, line));
    } catch (const std::invalid_argument& e) {
      FATAL(absl::StrFormat("Invalid argument at line %d: %s", line_num, line));
    } catch (const std::exception& e) {
      FATAL(absl::StrFormat("Failed to parse line %d: %s", line_num, line));
    }
  }
}

} /* namespace common */
} /* namespace jessie_steamer */
