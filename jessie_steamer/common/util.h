//
//  util.h
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_UTIL_H
#define JESSIE_STEAMER_COMMON_UTIL_H

#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "third_party/freetype/ft2build.h"
#include FT_FREETYPE_H
#include "third_party/glm/glm.hpp"

#define ASSERT_SUCCESS(event, error)                                          \
  if (event != VK_SUCCESS) {                                                  \
    throw std::runtime_error{"Error " + std::to_string(event) + ": " error};  \
  }
#define CONTAINER_SIZE(container)                                             \
  static_cast<uint32_t>(container.size())
#define INSERT_DEBUG_REQUIREMENT(overwrite)                                   \
  if (argc != 3) {                                                            \
    std::cout << "Usage: " << argv[0]                                         \
              << " <VK_ICD_FILENAMES> <VK_LAYER_PATH>" << std::endl;          \
    return EXIT_FAILURE;                                                      \
  } else {                                                                    \
    setenv("VK_ICD_FILENAMES", argv[1], overwrite);                           \
    setenv("VK_LAYER_PATH", argv[2], overwrite);                              \
  }

namespace jessie_steamer {
namespace common {
namespace util {

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

const uint32_t nullflag = 0;

TimePoint Now();

float TimeInterval(const TimePoint& t1, const TimePoint& t2);

template <typename AttribType>
std::vector<AttribType> QueryAttribute(
    const std::function<void(uint32_t*, AttribType*)>& enumerate) {
  uint32_t count;
  enumerate(&count, nullptr);
  std::vector<AttribType> attribs{count};
  enumerate(&count, attribs.data());
  return attribs;
}

template <typename AttribType>
void CheckSupport(
    const std::vector<std::string>& required,
    const std::vector<AttribType>& attribs,
    const std::function<const char*(const AttribType&)>& get_name) {
  std::unordered_set<std::string> available{attribs.size()};
  for (const auto& atr : attribs) {
    available.emplace(get_name(atr));
  }

  std::cout << "Available:" << std::endl;
  for (const auto& avl : available) {
    std::cout << "\t" << avl << std::endl;
  }
  std::cout << std::endl;

  std::cout << "Required:" << std::endl;
  for (const auto& req : required) {
    std::cout << "\t" << req << std::endl;
  }
  std::cout << std::endl;

  for (const auto& req : required) {
    if (available.find(req) == available.end()) {
      throw std::runtime_error{"Requirement not satisfied: " + req};
    }
  }
}

template <typename ContainerType>
void MoveAll(ContainerType* dst, ContainerType* src) {
  using MoveIterator =
      typename std::move_iterator<typename ContainerType::iterator>;
  dst->insert(dst->end(),
              MoveIterator(src->begin()),
              MoveIterator(src->end()));
}

extern const size_t kInvalidIndex;
template <typename ContentType>
size_t FindFirst(const std::vector<ContentType>& container,
                 const std::function<bool(const ContentType&)>& predicate) {
  auto first_itr = find_if(container.begin(), container.end(), predicate);
  return first_itr == container.end() ?
      kInvalidIndex : std::distance(container.begin(), first_itr);
}

const std::string& LoadTextFromFile(const std::string& path);

struct FileContent {
  size_t size;
  char* data;

  explicit FileContent(size_t size) : size{size}, data{new char[size]} {}
  ~FileContent() { delete[] data; }

  // This class is neither copyable nor movable
  FileContent(const FileContent&) = delete;
  FileContent& operator=(const FileContent&) = delete;
};

std::unique_ptr<FileContent> LoadRawDataFromFile(const std::string& path);

struct VertexAttrib2D {
  glm::vec2 pos;
  glm::vec2 tex_coord;

  VertexAttrib2D(const glm::vec2& pos,
                 const glm::vec2& tex_coord)
   : pos{pos}, tex_coord{tex_coord} {}
};

struct VertexAttrib3D {
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;

  VertexAttrib3D(const glm::vec3& pos,
                 const glm::vec3& norm,
                 const glm::vec2& tex_coord)
      : pos{pos}, norm{norm}, tex_coord{tex_coord} {}
};

void LoadObjFromFile(const std::string& path,
                     unsigned int index_base,
                     std::vector<VertexAttrib3D>* vertices,
                     std::vector<uint32_t>* indices);

class CharLib {
 public:
  struct Character {
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
    unsigned char* data;
  };

  CharLib(const std::vector<std::string>& texts,
          const std::string& font_path,
          glm::uvec2 font_size);
  ~CharLib();

  // This class is neither copyable nor movable
  CharLib(const CharLib&) = delete;
  CharLib& operator=(const CharLib&) = delete;

  const Character& operator[](char c) { return chars_[c]; }

 private:
  std::unordered_map<char, Character> chars_;
  FT_Library lib_;
  FT_Face face_;
};

struct Image {
  int width;
  int height;
  int channel;
  const void* data;

  explicit Image(const std::string& path);
  ~Image();

  // This class is only movable
  Image(Image&& rhs) noexcept {
    width = rhs.width;
    height = rhs.height;
    channel = rhs.channel;
    data = rhs.data;
    rhs.data = nullptr;
  }

  Image& operator=(Image&& rhs) noexcept {
    width = rhs.width;
    height = rhs.height;
    channel = rhs.channel;
    data = rhs.data;
    rhs.data = nullptr;
    return *this;
  }
};

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_UTIL_H */
