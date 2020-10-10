//
//  file.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_FILE_H
#define LIGHTER_COMMON_FILE_H

#include <array>
#include <string>
#include <vector>

#include "third_party/absl/flags/declare.h"
#include "third_party/absl/flags/flag.h"
#include "third_party/absl/strings/str_cat.h"
#include "third_party/glm/glm.hpp"

ABSL_DECLARE_FLAG(std::string, resource_folder);
ABSL_DECLARE_FLAG(std::string, shader_folder);
#ifdef USE_VULKAN
ABSL_DECLARE_FLAG(std::string, vulkan_folder);
#endif /* USE_VULKAN */

namespace lighter {
namespace common {
namespace file {

// Returns the full path to files in the resource folder.
inline std::string GetResourcePath(const std::string& relative_path) {
  return absl::StrCat(absl::GetFlag(FLAGS_resource_folder), "/", relative_path);
}

#ifdef USE_OPENGL
// Returns the full path to the compiled shader to use with OpenGL.
inline std::string GetGlShaderPath(const std::string& relative_path) {
  return absl::StrCat(absl::GetFlag(FLAGS_shader_folder), "/opengl/",
                      relative_path, ".spv");
}
#endif /* USE_OPENGL */

#ifdef USE_VULKAN
// Returns the full path to the compiled shader to use with Vulkan.
inline std::string GetVkShaderPath(const std::string& relative_path) {
  return absl::StrCat(absl::GetFlag(FLAGS_shader_folder), "/vulkan/",
                      relative_path, ".spv");
}

// Returns the full path to files in the Vulkan SDK folder.
inline std::string GetVulkanSdkPath(const std::string& relative_path) {
  return absl::StrCat(absl::GetFlag(FLAGS_vulkan_folder), "/", relative_path);
}
#endif /* USE_VULKAN */

} /* namespace file */

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

// Describes a vertex input attribute.
struct VertexAttribute {
  enum class DataType { kFloat };

  int offset;
  DataType data_type;
  int length;
};

// 2D vertex data, including only position.
struct Vertex2DPosOnly {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Returns vertices in normalized device coordinate for rendering a
  // full-screen squad.
  static std::array<Vertex2DPosOnly, 6> GetFullScreenSquadVertices();

  // Vertex data.
  glm::vec2 pos;
};

// 2D vertex data, consisting of position and texture coordinates.
struct Vertex2D {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Returns vertices in normalized device coordinate for rendering a
  // full-screen squad.
  static std::array<Vertex2D, 6> GetFullScreenSquadVertices(bool flip_y);

  // Vertex data.
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

// 3D vertex data, including only position.
struct Vertex3DPosOnly {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Vertex data.
  glm::vec3 pos;
};

// 3D vertex data, consisting of position and color.
struct Vertex3DWithColor {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Vertex data.
  glm::vec3 pos;
  glm::vec3 color;
};

// 3D vertex data, consisting of position, normal and texture coordinates.
struct Vertex3DWithTex {
  // Returns vertex input attributes.
  static std::vector<VertexAttribute> GetVertexAttributes();

  // Vertex data.
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;
};

namespace file {

// Appends vertex input attributes of DataType to 'attributes'. This is used for
// vector types with floating point values, such as glm::vec3 and glm::vec4.
template <typename DataType>
void AppendVertexAttributes(std::vector<VertexAttribute>& attributes,
                            int offset_bytes) {
  attributes.push_back(
      {offset_bytes, VertexAttribute::DataType::kFloat, DataType::length()});
}

// Appends vertex input attributes of glm::mat4 to 'attributes'.
template <>
void AppendVertexAttributes<glm::mat4>(std::vector<VertexAttribute>& attributes,
                                       int offset_bytes);

// Convenience function to return vertex input attributes for the data that has
// only one attribute of DataType.
template <typename DataType>
std::vector<VertexAttribute> CreateVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  AppendVertexAttributes<DataType>(attributes, /*offset_bytes=*/0);
  return attributes;
}

} /* namespace file */

// Loads Wavefront .obj file.
struct ObjFile {
  ObjFile(const std::string& path, int index_base);

  // This class is neither copyable nor movable.
  ObjFile(const ObjFile&) = delete;
  ObjFile& operator=(const ObjFile&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<uint32_t> indices;
  std::vector<Vertex3DWithTex> vertices;
};

// Loads Wavefront .obj file but only preserves vertex positions.
struct ObjFilePosOnly {
  ObjFilePosOnly(const std::string& path, int index_base);

  // This class is neither copyable nor movable.
  ObjFilePosOnly(const ObjFilePosOnly&) = delete;
  ObjFilePosOnly& operator=(const ObjFilePosOnly&) = delete;

  // Vertex data, populated with data loaded from the file.
  std::vector<uint32_t> indices;
  std::vector<Vertex3DPosOnly> vertices;
};

} /* namespace common */
} /* namespace lighter */

#endif /* LIGHTER_COMMON_FILE_H */
