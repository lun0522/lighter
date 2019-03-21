//
//  util.cc
//
//  Created by Pujun Lun on 2/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "util.h"

#include <array>
#include <sstream>
#include <unordered_map>

namespace util {
namespace {

using std::ifstream;
using std::runtime_error;
using std::stof;
using std::string;
using std::to_string;
using std::vector;

template <size_t N>
std::array<string, N> SplitText(const string& text, char delimiter) {
  std::istringstream stream{text};
  std::array<string, N> result;
  for (size_t i = 0; i < N; ++i) {
    if (!std::getline(stream, result[i], delimiter)) {
      throw runtime_error{"No enough segments (expected " +
                          to_string(N) + ", but only get " +
                          to_string(i) + ")"};
    }
  }
  return result;
}

} /* namespace */

const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

const string& ReadFile(const string& path) {
  static std::unordered_map<string, string> kLoadedText;
  auto loaded = kLoadedText.find(path);
  if (loaded == kLoadedText.end()) {
    ifstream file{path};
    file.exceptions(ifstream::failbit | ifstream::badbit);
    if (!file.is_open()) {
      throw runtime_error{"Failed to open file: " + path};
    }

    try {
      std::ostringstream stream;
      stream << file.rdbuf();
      string code = stream.str();
      loaded = kLoadedText.insert({path, code}).first;
    } catch (ifstream::failure e) {
      throw runtime_error{"Failed to read file: " + e.code().message()};
    }
  }
  return loaded->second;
}

void LoadObjFile(const string& path,
                 int index_base,
                 vector<VertexAttrib>& vertices,
                 vector<uint32_t>& indices) {
  std::ifstream file{path};
  if (!file.is_open()) {
    throw runtime_error{"Failed to open file: " + path};
  }

  vector<glm::vec3> positions;
  vector<glm::vec3> normals;
  vector<glm::vec2> tex_coords;
  std::unordered_map<string, uint32_t> loaded_vertices;

  auto parse_line = [&](const string& line) {
    size_t non_space = line.find_first_not_of(' ');
    if (non_space == string::npos || line[0] == '#') {
      return;  // skip blank lines and comments
    }

    switch (line[non_space]) {
      case 'v': {  // pos, norm or tex_coord
        switch (line[non_space + 1]) {
          case ' ': {  // pos
            auto nums = SplitText<3>(line.substr(non_space + 2), ' ');
            positions.emplace_back(stof(nums[0]), stof(nums[1]), stof(nums[2]));
            break;
          }
          case 'n': {  // norm
            auto nums = SplitText<3>(line.substr(non_space + 3), ' ');
            normals.emplace_back(stof(nums[0]), stof(nums[1]), stof(nums[2]));
            break;
          }
          case 't': {  // tex_coord
            auto nums = SplitText<2>(line.substr(non_space + 3), ' ');
            tex_coords.emplace_back(stof(nums[0]), stof(nums[1]));
            break;
          }
          default: {
            throw runtime_error{"Unrecognized symbol"};
          }
        }
        break;
      }
      case 'f': {  // face
        auto segs = SplitText<3>(line.substr(non_space + 2), ' ');
        for (const auto& seg : segs) {
          auto found = loaded_vertices.find(seg);
          if (found != loaded_vertices.end()) {
            indices.emplace_back(found->second);
          } else {
            indices.emplace_back(vertices.size());
            loaded_vertices.emplace(seg, vertices.size());
            auto idxs = SplitText<3>(seg, '/');
            vertices.emplace_back(
                positions.at(stoi(idxs[0]) - index_base),
                normals.at(stoi(idxs[2]) - index_base),
                tex_coords.at(stoi(idxs[1]) - index_base)
            );
          }
        }
        break;
      }
      default: {
        throw runtime_error{"Unrecognized symbol"};
      }
    }
  };

  size_t line_num = 1;
  for (string line; getline(file, line); ++line_num) {
    try {
      parse_line(line);
    } catch (const std::out_of_range& e) {
      throw runtime_error{"Out of range error at line " +
                          to_string(line_num) + ": " + line};
    } catch (const std::invalid_argument& e) {
      throw runtime_error{"Invalid argument at line " +
                          to_string(line_num) + ": " + line};
    } catch (const std::exception& e) {
      throw runtime_error{"Failed to parse line " +
                          to_string(line_num) + ": " + line};
    }
  }
}

} /* namespace util */
