//
//  data.h
//
//  Created by Pujun Lun on 10/2/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_DATA_H
#define LIGHTER_COMMON_DATA_H

#include <array>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "lighter/common/util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/glm.hpp"

namespace lighter::common {

// Holds a contiguous memory block. The memory will be freed when destructed.
class Data {
 public:
  Data(void* data, size_t size)
      : data_{data == nullptr ? std::malloc(size) : data}, size_{size} {}
  explicit Data(size_t size) : Data{nullptr, size} {}
  explicit Data() : Data{nullptr, 0} {}

  // This class is only movable.
  Data(Data&& rhs) noexcept {
    data_ = rhs.data_;
    size_ = rhs.size_;
    rhs.data_ = nullptr;
  }

  Data& operator=(Data&& rhs) noexcept {
    std::swap(data_, rhs.data_);
    std::swap(size_, rhs.size_);
    return *this;
  }

  virtual ~Data() { std::free(data_); }

  // Accessors.
  size_t size() const { return size_; }
  template <typename T>
  T* mut_data() { return static_cast<T*>(data_); }
  template <typename T>
  const T* data() const { return static_cast<const T*>(data_); }

 private:
  void* data_;
  size_t size_;
};

// This is the base class of the classes holding contiguous data chunks.
class ChunkedData : public Data {
 protected:
  ChunkedData(void* data, size_t chunk_size, int num_chunks)
      : Data{data, chunk_size * num_chunks}, num_chunks_{num_chunks} {}

  void ValidateChunkIndex(int chunk_index) const {
    ASSERT_TRUE(
      chunk_index < num_chunks_,
      absl::StrFormat("Trying to access chunk %d, while only %d chunks exist",
                      chunk_index, num_chunks_));
  }

 private:
  int num_chunks_;
};

// Holds contiguous stored raw data chunks.
class RawChunkedData : public ChunkedData {
 public:
  RawChunkedData(void* data, size_t chunk_size, int num_chunks)
      : ChunkedData{data, chunk_size, num_chunks}, chunk_size_{chunk_size} {}

  RawChunkedData(size_t chunk_size, int num_chunks)
      : RawChunkedData{nullptr, chunk_size, num_chunks} {}

  char* GetMutData(int chunk_index) {
    ValidateChunkIndex(chunk_index);
    return mut_data<char>() + chunk_size_ * chunk_index;
  }

  const char* GetData(int chunk_index) const {
    ValidateChunkIndex(chunk_index);
    return data<char>() + chunk_size_ * chunk_index;
  }

  // Copies `chunk_size_` of data from `source`.
  void CopyChunkFrom(const void* source, int chunk_index) {
    std::memcpy(GetMutData(chunk_index), source, chunk_size_);
  }

  // Accessors.
  size_t chunk_size() const { return chunk_size_; }

 private:
  size_t chunk_size_;
};

// Holds contiguous stored data chunks where each chunk stores an instance of T.
template <typename T>
class TypedChunkedData : public ChunkedData {
 public:
  explicit TypedChunkedData(int num_chunks)
      : ChunkedData{nullptr, sizeof(T), num_chunks} {}

  T* GetMutData(int chunk_index) {
    ValidateChunkIndex(chunk_index);
    return &mut_data<T>()[chunk_index];
  }

  const T* GetData(int chunk_index) const {
    ValidateChunkIndex(chunk_index);
    return &data<T>()[chunk_index];
  }
};

// TODO: Remove this struct and related methods.
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

namespace data {

// Appends vertex input attributes of DataType to `attributes`. This is used for
// vector types with floating point values, such as glm::vec3 and glm::vec4.
template <typename DataType>
void AppendVertexAttributes(std::vector<VertexAttribute>& attributes,
                            int offset_bytes) {
  attributes.push_back(
      {offset_bytes, VertexAttribute::DataType::kFloat, DataType::length()});
}

// Appends vertex input attributes of glm::mat4 to `attributes`.
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

}  // namespace data
}  // namespace lighter::common

#endif  // LIGHTER_COMMON_DATA_H
