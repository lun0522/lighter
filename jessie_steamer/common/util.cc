//
//  util.cc
//
//  Created by Pujun Lun on 2/17/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/util.h"

#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

namespace jessie_steamer {
namespace common {
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

ifstream OpenFile(const string& path) {
  ifstream file{path};
  if (!file.is_open() || file.bad() || file.fail()) {
    throw runtime_error{"Failed to open file: " + path};
  }
  return file;
}

} /* namespace */

TimePoint Now() {
  return std::chrono::high_resolution_clock::now();
}

float TimeInterval(const TimePoint& t1, const TimePoint& t2) {
  return std::chrono::duration<float, std::chrono::seconds::period>(
      t2 - t1).count();
}

const size_t kInvalidIndex = std::numeric_limits<size_t>::max();

const string& LoadTextFromFile(const string &path) {
  static std::unordered_map<string, string> kLoadedText;
  auto loaded = kLoadedText.find(path);
  if (loaded == kLoadedText.end()) {
    try {
      ifstream file = OpenFile(path);
      std::ostringstream stream;
      stream << file.rdbuf();
      string text = stream.str();
      loaded = kLoadedText.emplace(path, std::move(text)).first;
    } catch (const ifstream::failure& e) {
      throw runtime_error{"Failed to read file: " + e.code().message()};
    }
  }
  return loaded->second;
}

FileContent LoadRawDataFromFile(const string& path) {
  ifstream file = OpenFile(path);
  file.seekg(0, std::ios::end);
  FileContent content{static_cast<size_t>(file.tellg())};
  file.seekg(0, std::ios::beg);
  file.read(content.data, content.size);
  return content;
}

void LoadObjFromFile(const string& path,
                     int index_base,
                     vector<VertexAttrib3D>* vertices,
                     vector<uint32_t>* indices) {
  ifstream file = OpenFile(path);

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
            indices->emplace_back(found->second);
          } else {
            indices->emplace_back(vertices->size());
            loaded_vertices.emplace(seg, vertices->size());
            auto idxs = SplitText<3>(seg, '/');
            vertices->emplace_back(
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

CharLib::CharLib(const vector<string>& texts,
                 const string &font_path,
                 glm::uvec2 font_size) {
  if (FT_Init_FreeType(&lib_)) {
    throw runtime_error{"Failed to init FreeType library"};
  }

  if (FT_New_Face(lib_, font_path.c_str(), 0, &face_)) {
    throw runtime_error{"Failed to load font"};
  }
  FT_Set_Pixel_Sizes(face_, font_size.x, font_size.y);

  std::unordered_set<char> to_load;
  for (const auto& text : texts) {
    for (auto c : text) {
      to_load.emplace(c);
    }
  }

  for (auto c : to_load) {
    if (FT_Load_Char(face_, c, FT_LOAD_RENDER)) {
      throw runtime_error{"Failed to load glyph"};
    }

    Character ch{
        /*size=*/{face_->glyph->bitmap.width,
                  face_->glyph->bitmap.rows},
        /*bearing=*/{face_->glyph->bitmap_left,
                     face_->glyph->bitmap_top},
        // measured with number of 1/64 pixels
        /*advance=*/(unsigned int)face_->glyph->advance.x >> 6,
        /*data=*/face_->glyph->bitmap.buffer,
    };
    chars_.emplace(c, std::move(ch));
  }
}

CharLib::~CharLib() {
  FT_Done_Face(face_);
  FT_Done_FreeType(lib_);
}

Image::Image(const string& path) {
  FileContent content = LoadRawDataFromFile(path);
  data = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(content.data),
                               content.size, &width, &height, &channel,
                               STBI_default);
  if (data == nullptr) {
    throw runtime_error{"Failed to read image from " + path};
  }

  switch (channel) {
    case 1:
    case 4:
      break;
    case 3: {  // force to have alpha channel
      stbi_image_free(const_cast<void*>(data));
      data = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(content.data),
                                   content.size, &width, &height, &channel,
                                   STBI_rgb_alpha);
      channel = STBI_rgb_alpha;
      break;
    }
    default:
      throw runtime_error{"Unsupported number of channels: " +
                          std::to_string(channel)};
  }
}

Image::~Image() {
  stbi_image_free(const_cast<void*>(data));
}

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */
