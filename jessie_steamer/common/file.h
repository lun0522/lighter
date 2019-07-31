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

// Reads raw data from file.
struct RawData {
  explicit RawData(const std::string& path);

  // This class is neither copyable nor movable.
  RawData(const RawData&) = delete;
  RawData& operator=(const RawData&) = delete;

  ~RawData() { delete[] data; }

  // Pointer to data.
  const char* data;

  // Data size.
  size_t size;
};

// Loads image from file or memory.
struct Image {
  // Loads image from file. The image can have either 1, 3, or 4 channels.
  // If the image has 3 channels, we will reload and assign it the 4th channel.
  explicit Image(const std::string& path);

  // Loads image from memory. The data will be copied, hence the caller may free
  // original data once the constructor returns.
  // The image can have either 1 or 4 channels.
  Image(int width, int height, int channel, const void* raw_data, bool flip_y);

  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  ~Image();

  // Dimensions of image.
  int width;
  int height;
  int channel;

  // Pointer to data.
  const void* data;
};

// 2D vertex data, consisting of position and texture coordinates.
struct VertexAttrib2D {
  VertexAttrib2D(const glm::vec2& pos, const glm::vec2& tex_coord)
      : pos{pos}, tex_coord{tex_coord} {}

  // Vertex data.
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

// 3D vertex data, consisting of position, normal and texture coordinates.
struct VertexAttrib3D {
  VertexAttrib3D(const glm::vec3& pos,
                 const glm::vec3& norm,
                 const glm::vec2& tex_coord)
      : pos{pos}, norm{norm}, tex_coord{tex_coord} {}

  // Vertex data.
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;
};

// Loads Wavefront .obj file.
struct ObjFile {
  ObjFile(const std::string& path, int index_base);

  // This class is neither copyable nor movable.
  ObjFile(const ObjFile&) = delete;
  ObjFile& operator=(const ObjFile&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<VertexAttrib3D> vertices;
  std::vector<uint32_t> indices;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_FILE_H */
