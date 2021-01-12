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

namespace lighter::common {
namespace image {

constexpr int kSingleMipLevel = 1;
constexpr int kSingleImageLayer = 1;
constexpr int kCubemapImageLayer = 6;

constexpr int kBwImageChannel = 1;
constexpr int kRgbImageChannel = 3;
constexpr int kRgbaImageChannel = 4;

}  // namespace image

// Loads image from file or memory.
class Image {
 public:
  // Dimension of image data.
  struct Dimension {
    int width;
    int height;
    int channel;
    int layer;
  };

  // Loads images from files. All images are expected to have the same width,
  // height and channel. The number of channels may be either 1, 3 or 4. If it
  // is 3, we will create the 4th channel internally.
  // The length of 'paths' and 'relative_paths' must be either 1 or 6 (cubemap).
  explicit Image(absl::Span<const std::string> paths);
  explicit Image(const std::string& path) : Image{{&path, 1}} {}
  explicit Image(std::string_view path) : Image{std::string{path}} {}
  Image(std::string_view directory,
        absl::Span<const std::string> relative_paths);

  // Loads images from the memory. The data will be copied, hence the caller may
  // free the original data once the constructor returns.
  // Images may have either 1 or 4 channels.
  // The length of 'raw_data_ptrs' must be either 1 or 6.
  Image(int width, int height, int channel,
        absl::Span<const void* const> raw_data_ptrs, bool flip_y);
  Image(int width, int height, int channel, const void* raw_data, bool flip_y)
      : Image{width, height, channel, {&raw_data, 1}, flip_y} {}

  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  ~Image();

  // Accessors.
  const Dimension& dimension() const { return dimension_; }
  int width() const { return dimension_.width; }
  int height() const { return dimension_.height; }
  int channel() const { return dimension_.channel; }
  int layer() const { return dimension_.layer; }
  const std::vector<const void*>& data_ptrs() const { return data_ptrs_; }

 private:
  // Loads image from 'path' and returns the pointer to image data. It also
  // checks that the dimension of image matches 'dimension_'.
  const void* LoadImageFromFile(std::string_view path) const;

  // Dimension of image data.
  Dimension dimension_{};

  // Pointers to image data.
  std::vector<const void*> data_ptrs_;
};

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_IMAGE_H
