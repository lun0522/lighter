//
//  file.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_FILE_H
#define JESSIE_STEAMER_COMMON_FILE_H

#include <string>
#include <vector>

#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

struct RawData {
  explicit RawData(const std::string& path);

  // This class is neither copyable nor movable.
  RawData(const RawData&) = delete;
  RawData& operator=(const RawData&) = delete;

  ~RawData() { delete[] data; }

  size_t size;
  const char* data;
};

struct Image {
  explicit Image(const std::string& path);

  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  ~Image();

  int width;
  int height;
  int channel;
  const void* data;
};

struct VertexAttrib2D {
  VertexAttrib2D(const glm::vec2& pos, const glm::vec2& tex_coord)
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

namespace file {

void LoadObjFile(const std::string& path,
                 unsigned int index_base,
                 std::vector<VertexAttrib3D>* vertices,
                 std::vector<uint32_t>* indices);

} /* namespace file */
} /* namespace common */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_COMMON_FILE_H
