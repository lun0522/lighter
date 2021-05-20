//
//  image.cc
//
//  Created by Pujun Lun on 10/20/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/image.h"

#include <cstdlib>

#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "third_party/absl/strings/str_cat.h"
#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb/stb_image.h"

namespace lighter::common {
namespace {

// Concatenates 'directory' and each of 'relative_paths'.
std::vector<std::string> GetFullPaths(
    std::string_view directory,
    absl::Span<const std::string> relative_paths) {
  std::vector<std::string> paths;
  paths.reserve(relative_paths.size());
  for (const auto& path : relative_paths) {
    paths.push_back(absl::StrCat(directory, "/", path));
  }
  return paths;
}

}  // namespace

Image::Image(absl::Span<const std::string> paths) {
  ASSERT_TRUE(paths.size() == image::kSingleImageLayer
                  || paths.size() == image::kCubemapImageLayer,
              absl::StrFormat("Unsupported number of images: %d",
                              paths.size()));

  // Load the first image. The rest of images are expected to have the same
  // dimension to it.
  const auto raw_data = std::make_unique<RawData>(paths[0]);
  stbi_uc* data = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc*>(raw_data->data),
      static_cast<int>(raw_data->size),
      &dimension_.width, &dimension_.height, &dimension_.channel, STBI_default);
  ASSERT_NON_NULL(
      data, absl::StrFormat("Failed to read image from %s", paths[0]));

  // If the image has 3 channels, reload it so that it has 4 channels.
  switch (channel()) {
    case image::kBwImageChannel:
    case image::kRgbaImageChannel:
      break;

    case image::kRgbImageChannel: {
      stbi_image_free(data);
      data = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(raw_data->data),
          static_cast<int>(raw_data->size),
          &dimension_.width, &dimension_.height, &dimension_.channel,
          STBI_rgb_alpha);
      dimension_.channel = STBI_rgb_alpha;
      break;
    }

    default:
      FATAL(absl::StrFormat("Unsupported number of channels: %d",
                            dimension_.channel));
  }

  // Load the rest of images.
  data_ptrs_.reserve(paths.size());
  data_ptrs_.push_back(data);
  paths.remove_prefix(1);
  for (const auto& path : paths) {
    data_ptrs_.push_back(LoadImageFromFile(path));
  }
}

Image::Image(std::string_view directory,
             absl::Span<const std::string> relative_paths)
    : Image{GetFullPaths(directory, relative_paths)} {}

Image::Image(int width, int height, int channel,
             absl::Span<const void* const> raw_data_ptrs, bool flip_y) {
  ASSERT_TRUE(channel == image::kBwImageChannel
                  || channel == image::kRgbaImageChannel,
              absl::StrFormat("Unsupported number of channels: %d", channel));
  ASSERT_TRUE(raw_data_ptrs.size() == image::kSingleImageLayer
                  || raw_data_ptrs.size() == image::kCubemapImageLayer,
              absl::StrFormat("Unsupported number of images: %d",
                              raw_data_ptrs.size()));
  dimension_ = {width, height, channel, static_cast<int>(raw_data_ptrs.size())};
  data_ptrs_.reserve(layer());

  const size_t data_size_per_layer = width * height * channel;
  for (const auto* raw_data : raw_data_ptrs) {
    void* data = std::malloc(data_size_per_layer);
    if (flip_y) {
      const size_t stride = width * channel;
      for (int row = 0; row < height; ++row) {
        std::memcpy(
            static_cast<char*>(data) + stride * row,
            static_cast<const char*>(raw_data) + stride * (height - row - 1),
            stride);
      }
    } else {
      std::memcpy(data, raw_data, data_size_per_layer);
    }
    data_ptrs_.push_back(data);
  }
}

Image::~Image() {
  for (const auto* data : data_ptrs_) {
    std::free(const_cast<void*>(data));
  }
}

const void* Image::LoadImageFromFile(std::string_view path) const {
  const auto raw_data = std::make_unique<RawData>(path);
  int width, height, channel;
  stbi_uc* data = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc*>(raw_data->data),
      static_cast<int>(raw_data->size),
      &width, &height, &channel, /*desired_channels=*/this->channel());

  ASSERT_NON_NULL(data, absl::StrFormat("Failed to read image from %s", path));
  ASSERT_TRUE(width == this->width(),
              absl::StrFormat("Image loaded from %s has different width (%d vs "
                              "%d from first image)",
                              path, width, this->width()));
  ASSERT_TRUE(height == this->height(),
              absl::StrFormat("Image loaded from %s has different height (%d "
                              "vs %d from first image)",
                              path, height, this->height()));

  return data;
}

}  // namespace lighter::common
