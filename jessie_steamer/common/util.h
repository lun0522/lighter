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
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/optional.h"
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

const uint32_t nullflag = 0;

class Timer {
 public:
  Timer() : frame_count_{0} {
    launch_time_ = last_second_time_ = last_frame_time_ = Now();
  }

  absl::optional<size_t> frame_rate() {
    ++frame_count_;
    last_frame_time_ = Now();
    absl::optional<size_t> ret = absl::nullopt;
    if (TimeInterval(last_second_time_, last_frame_time_) >= 1.0f) {
      last_second_time_ = last_frame_time_;
      ret = absl::make_optional<size_t>(frame_count_);
      frame_count_ = 0;
    }
    return ret;
  }

  float time_from_launch() const {
    return TimeInterval(launch_time_, Now());
  }

  float time_from_last_frame() const {
    return TimeInterval(last_frame_time_, Now());
  }

 private:
  using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

  static TimePoint Now() { return std::chrono::high_resolution_clock::now(); }

  static float TimeInterval(const TimePoint& t1, const TimePoint& t2) {
    return std::chrono::duration<
        float, std::chrono::seconds::period>(t2 - t1).count();
  }

  TimePoint launch_time_;
  TimePoint last_second_time_;
  TimePoint last_frame_time_;
  size_t frame_count_;
};

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
  absl::flat_hash_set<std::string> available{attribs.size()};
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
  VertexAttrib2D(const glm::vec2& pos,
                 const glm::vec2& tex_coord)
   : pos{pos}, tex_coord{tex_coord} {}

  glm::vec2 pos;
  glm::vec2 tex_coord;
};

struct VertexAttrib3D {
  VertexAttrib3D(const glm::vec3& pos,
                 const glm::vec3& norm,
                 const glm::vec2& tex_coord)
      : pos{pos}, norm{norm}, tex_coord{tex_coord} {}

  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;
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

  // This class is neither copyable nor movable
  CharLib(const CharLib&) = delete;
  CharLib& operator=(const CharLib&) = delete;

  ~CharLib();

  const Character& operator[](char c) { return chars_[c]; }

 private:
  absl::flat_hash_map<char, Character> chars_;
  FT_Library lib_;
  FT_Face face_;
};

struct Image {
  Image() = default;

  // This class is neither copyable nor movable
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  void Init(const std::string& path);

  ~Image();

  int width;
  int height;
  int channel;
  const void* data;
};

} /* namespace util */
} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_UTIL_H */
