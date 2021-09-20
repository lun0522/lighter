//
//  image.h
//
//  Created by Pujun Lun on 10/20/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_IMAGE_H
#define LIGHTER_COMMON_IMAGE_H

#include <string>
#include <string_view>
#include <vector>

#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter::common {
namespace image {

constexpr int kSingleMipLevel = 1;
constexpr int kSingleImageLayer = 1;
constexpr int kCubemapImageLayer = 6;

constexpr int kBwImageChannel = 1;
constexpr int kRgbImageChannel = 3;
constexpr int kRgbaImageChannel = 4;

enum class Type { kSingle, kCubemap };

inline int GetNumLayers(Type type) {
  switch (type) {
    case Type::kSingle:
      return kSingleImageLayer;
    case Type::kCubemap:
      return kCubemapImageLayer;
  }
}

}  // namespace image

// Loads image from file or memory.
class Image {
 public:
  using Type = image::Type;

  // Dimension of image data.
  struct Dimension {
    int width;
    int height;
    int channel;

    glm::ivec2 extent() const { return {width, height}; }
    size_t size_per_layer() const { return width * height * channel; }
  };

  // Loads cubemaps from files. All images are expected to have the same width,
  // height and channel. The number of channels may be either 1, 3 or 4. If it
  // is 3, we will create the 4th channel internally.
  // The length of 'paths' and 'relative_paths' must be 6.
  static Image LoadCubemapFromFiles(absl::Span<const std::string> paths,
                                    bool flip_y);
  static Image LoadCubemapFromFiles(
        std::string_view directory,
        absl::Span<const std::string> relative_paths, bool flip_y);

  explicit Image() = default;

  // Loads an image from file. The number of channels may be either 1, 3 or 4.
  // If it is 3, we will create the 4th channel internally.
  Image(std::string_view path, bool flip_y);

  // Loads images from the memory. The data will be copied, hence the caller may
  // free the original data once the constructor returns.
  // Images may have either 1 or 4 channels.
  // The length of 'raw_data_ptrs' must be either 1 or 6 (cubemap).
  Image(const Dimension& dimension, absl::Span<const void* const> raw_data_ptrs,
        bool flip_y);
  Image(const Dimension& dimension, const void* raw_data, bool flip_y)
      : Image{dimension, {&raw_data, 1}, flip_y} {}

  // This class is only movable.
  Image(Image&& rhs) noexcept {
    type_ = rhs.type_;
    dimension_ = rhs.dimension_;
    data_ = rhs.data_;
    rhs.data_ = nullptr;
  }

  Image& operator=(Image&& rhs) noexcept {
    std::swap(type_, rhs.type_);
    std::swap(dimension_, rhs.dimension_);
    std::swap(data_, rhs.data_);
    return *this;
  }

  ~Image() { std::free(const_cast<void*>(data_)); }

  // Returns the number of image layers, which is determined by whether this is
  // a cubemap.
  int GetNumLayers() const { return image::GetNumLayers(type_); }

  // Returns a vector of pointers to the data of each image layer.
  std::vector<const void*> GetDataPtrs() const;

  // Accessors.
  Type type() const { return type_; }
  glm::ivec2 extent() const { return dimension_.extent(); }
  int width() const { return dimension_.width; }
  int height() const { return dimension_.height; }
  int channel() const { return dimension_.channel; }

 private:
  // Type of image.
  Type type_{};

  // Dimension of image data.
  Dimension dimension_{};

  // Pointer to all image data.
  const void* data_ = nullptr;
};

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_IMAGE_H
