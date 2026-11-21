#ifndef LIGHTER_BASE_DATA_H
#define LIGHTER_BASE_DATA_H

#include <array>
#include <cstdlib>
#include <cstring>
#include <format>
#include <fstream>
#include <glm/glm.hpp>
#include <vector>

#include "lighter/base/util.h"

// We use the uniform block layout std140 in all shaders. The following
// alignment requirements must be enforced on the host code:
// https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/chap14.html#interfaces-resources-layout
#define ALIGN_SCALAR(type) alignas(sizeof(type))
#define ALIGN_VEC4 alignas(sizeof(float) * 4)
#define ALIGN_MAT4 alignas(sizeof(float) * 4)

namespace lighter::base {

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
  T* mut_data() {
    return static_cast<T*>(data_);
  }
  template <typename T>
  const T* data() const {
    return static_cast<const T*>(data_);
  }

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
        std::format("Trying to access chunk {}, while only {} chunks exist",
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

// 2D vertex data, including only position.
struct Vertex2DPosOnly {
  // Returns vertices in normalized device coordinate for rendering a
  // full-screen squad.
  static std::array<Vertex2DPosOnly, 6> GetFullScreenSquadVertices();

  // Vertex data.
  glm::vec2 pos;
};

// 2D vertex data, consisting of position and texture coordinates.
struct Vertex2D {
  // Returns vertices in normalized device coordinate for rendering a
  // full-screen squad.
  static std::array<Vertex2D, 6> GetFullScreenSquadVertices(bool flip_y);

  // Vertex data.
  glm::vec2 pos;
  glm::vec2 tex_coord;
};

// 3D vertex data, including only position.
struct Vertex3DPosOnly {
  // Vertex data.
  glm::vec3 pos;
};

// 3D vertex data, consisting of position and color.
struct Vertex3DWithColor {
  // Vertex data.
  glm::vec3 pos;
  glm::vec3 color;
};

// 3D vertex data, consisting of position, normal and texture coordinates.
struct Vertex3DWithTex {
  // Vertex data.
  glm::vec3 pos;
  glm::vec3 norm;
  glm::vec2 tex_coord;
};

}  // namespace lighter::base

#endif  // LIGHTER_BASE_DATA_H
