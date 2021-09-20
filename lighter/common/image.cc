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

struct SingleImage {
  Image::Dimension dimension;
  const void* data;
};

// Loads one image from 'path'. If the image has 3 channels, reload it with 4
// channels internally. The caller is responsible for freeing the memory after
// done with the data.
SingleImage LoadSingleImage(std::string_view path, int desired_channels) {
  const RawData raw_data{path};
  int width, height, channel;
  stbi_uc* data = stbi_load_from_memory(
      reinterpret_cast<const stbi_uc*>(raw_data.data),
      static_cast<int>(raw_data.size), &width, &height, &channel,
      desired_channels);
  ASSERT_NON_NULL(
      data, absl::StrFormat("Failed to read image from '%s'", path));

  // If the image has 3 channels, reload it so that it has 4 channels.
  switch (channel) {
    case image::kBwImageChannel:
    case image::kRgbaImageChannel:
      break;

    case image::kRgbImageChannel: {
      stbi_image_free(data);
      data = stbi_load_from_memory(
          reinterpret_cast<const stbi_uc*>(raw_data.data),
          static_cast<int>(raw_data.size), &width, &height, &channel,
          STBI_rgb_alpha);
      channel = STBI_rgb_alpha;
      break;
    }

    default:
      FATAL(absl::StrFormat(
          "Unsupported number of channels (%d) when loading from %s",
          channel, path));
  }
  return {{width, height, channel}, data};
}

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

bool operator==(const Image::Dimension& lhs, const Image::Dimension& rhs) {
    return lhs.width == rhs.width &&
           lhs.height == rhs.height &&
           lhs.channel == rhs.channel;
}

}  // namespace

Image Image::LoadCubemapFromFiles(absl::Span<const std::string> paths,
                                  bool flip_y) {
  ASSERT_TRUE(paths.size() == image::kCubemapImageLayer,
              absl::StrFormat("Length of 'paths' (%d) is not 6", paths.size()));

  stbi_set_flip_vertically_on_load(flip_y);
  const auto first_image = LoadSingleImage(paths[0], STBI_default);
  std::vector<const void*> data_ptrs(paths.size());
  data_ptrs[0] = first_image.data;
  for (int i = 1; i < paths.size(); ++i) {
    const auto& path = paths[i];
    const auto image = LoadSingleImage(path, first_image.dimension.channel);
    ASSERT_TRUE(image.dimension == first_image.dimension,
                absl::StrFormat("Image loaded from %s has different dimension "
                                "compared with the first image from %s",
                                path, paths[0]));
    data_ptrs[i] = image.data;
  }
  stbi_set_flip_vertically_on_load(false);

  Image loaded_image{first_image.dimension, data_ptrs, /*flip_y=*/false};
  for (const void* ptr : data_ptrs) {
    std::free(const_cast<void*>(ptr));
  }
  return loaded_image;
}

Image Image::LoadCubemapFromFiles(
    std::string_view directory, absl::Span<const std::string> relative_paths,
    bool flip_y) {
  ASSERT_TRUE(relative_paths.size() == image::kCubemapImageLayer,
              absl::StrFormat("Length of 'relative_paths' (%d) is not 6",
                              relative_paths.size()));
  return LoadCubemapFromFiles(GetFullPaths(directory, relative_paths), flip_y);
}

Image::Image(std::string_view path, bool flip_y) : type_{Type::kSingle} {
  stbi_set_flip_vertically_on_load(flip_y);
  const auto image = LoadSingleImage(path, STBI_default);
  stbi_set_flip_vertically_on_load(false);

  dimension_ = image.dimension;
  data_ = image.data;
} 

Image::Image(const Dimension& dimension,
             absl::Span<const void* const> raw_data_ptrs, bool flip_y) {
  const int num_layers = raw_data_ptrs.size();
  switch (num_layers) {
    case image::kSingleImageLayer:
      type_ = Type::kSingle;
      break;
    case image::kCubemapImageLayer:
      type_ = Type::kCubemap;
      break;
    default:
      FATAL(absl::StrFormat("Unsupported number of layers: %d", num_layers));
  }

  const int channel = dimension.channel;
  ASSERT_TRUE(channel == image::kBwImageChannel
                  || channel == image::kRgbaImageChannel,
              absl::StrFormat("Unsupported number of channels: %d", channel));
  dimension_ = dimension;

  const size_t size_per_layer = dimension_.size_per_layer();
  char* copied_data =
      static_cast<char*>(std::malloc(size_per_layer * num_layers));
  for (const auto* raw_data : raw_data_ptrs) {
    if (flip_y) {
      const size_t stride = width() * channel;
      for (int row = 0; row < height(); ++row) {
        std::memcpy(
            copied_data + stride * row,
            static_cast<const char*>(raw_data) + stride * (height() - row - 1),
            stride);
      }
    } else {
      std::memcpy(copied_data, raw_data, size_per_layer);
    }
    copied_data += size_per_layer;
  }
  data_ = copied_data;
}

std::vector<const void*> Image::GetDataPtrs() const {
  const size_t size_per_layer = dimension_.size_per_layer();
  const int num_layers = GetNumLayers();
  std::vector<const void*> ptrs(num_layers);
  for (int layer = 0; layer < num_layers; ++layer) {
    ptrs[layer] = static_cast<const char*>(data_) + size_per_layer * layer;
  }
  return ptrs;
}

}  // namespace lighter::common
