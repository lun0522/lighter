#include "lighter/base/image.h"

#include <cstdlib>
#include <cstring>
#include <format>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "lighter/base/file.h"
#include "lighter/base/util.h"

namespace lighter::base {
namespace {

// Holds a loaded single image. `data` would only have one chunk.
struct SingleImage {
  const char* data_ptr() const { return data.GetData(/*chunk_index=*/0); }

  Image::Dimension dimension;
  RawChunkedData data;
};

// Loads one image from 'path'. If the image has 3 channels, reload it with 4
// channels internally.
SingleImage LoadSingleImage(std::string_view path, int desired_channels) {
  const Data file_data = file::LoadDataFromFile(path);
  int width, height, channel;
  stbi_uc* data = stbi_load_from_memory(
      file_data.data<stbi_uc>(), static_cast<int>(file_data.size()), &width,
      &height, &channel, desired_channels);
  ASSERT_NON_NULL(data, std::format("Failed to read image from '{}'", path));

  // If the image has 3 channels, reload it so that it has 4 channels.
  switch (channel) {
    case image::kBwImageChannel:
    case image::kRgbaImageChannel:
      break;

    case image::kRgbImageChannel: {
      stbi_image_free(data);
      data = stbi_load_from_memory(file_data.data<stbi_uc>(),
                                   static_cast<int>(file_data.size()), &width,
                                   &height, &channel, STBI_rgb_alpha);
      channel = STBI_rgb_alpha;
      break;
    }

    default:
      FATAL(std::format(
          "Unsupported number of channels ({}) when loading from {}", channel,
          path));
  }

  const Image::Dimension dimension{width, height, channel};
  return {
      dimension,
      RawChunkedData{data, dimension.size_per_layer(), /*num_chunks=*/1},
  };
}

// Concatenates 'directory' and each of 'relative_paths'.
std::vector<std::string> GetFullPaths(
    std::string_view directory, std::span<const std::string> relative_paths) {
  std::vector<std::string> paths;
  paths.reserve(relative_paths.size());
  for (const auto& path : relative_paths) {
    paths.push_back(std::format("{}/{}", directory, path));
  }
  return paths;
}

bool operator==(const Image::Dimension& lhs, const Image::Dimension& rhs) {
  return lhs.width == rhs.width && lhs.height == rhs.height &&
         lhs.channel == rhs.channel;
}

}  // namespace

namespace image {

int GetNumLayers(Type type) {
  switch (type) {
    case Type::kSingle:
      return kSingleImageLayer;
    case Type::kCubemap:
      return kCubemapImageLayer;
  }
}

}  // namespace image

Image Image::LoadSingleImageFromFile(std::string_view path, bool flip_y) {
  stbi_set_flip_vertically_on_load(flip_y);
  SingleImage image = LoadSingleImage(path, STBI_default);
  stbi_set_flip_vertically_on_load(false);
  return {Image::Type::kSingle, image.dimension, std::move(image.data)};
}

Image Image::LoadCubemapFromFiles(std::span<const std::string> paths,
                                  bool flip_y) {
  ASSERT_TRUE(paths.size() == image::kCubemapImageLayer,
              std::format("Length of 'paths' ({}) is not 6", paths.size()));

  stbi_set_flip_vertically_on_load(flip_y);
  const SingleImage first_image = LoadSingleImage(paths[0], STBI_default);
  const Dimension& dimension = first_image.dimension;
  RawChunkedData cubemap_data{dimension.size_per_layer(),
                              image::kCubemapImageLayer};
  cubemap_data.CopyChunkFrom(first_image.data_ptr(), /*chunk_index=*/0);
  for (int i = 1; i < paths.size(); ++i) {
    const std::string& path = paths[i];
    const SingleImage image = LoadSingleImage(path, dimension.channel);
    ASSERT_TRUE(image.dimension == dimension,
                std::format("Image loaded from {} has different dimension "
                            "compared with the first image from {}",
                            path, paths[0]));
    cubemap_data.CopyChunkFrom(image.data_ptr(), /*chunk_index=*/i);
  }
  stbi_set_flip_vertically_on_load(false);

  return {Image::Type::kCubemap, dimension, std::move(cubemap_data)};
}

Image Image::LoadCubemapFromFiles(std::string_view directory,
                                  std::span<const std::string> relative_paths,
                                  bool flip_y) {
  ASSERT_TRUE(relative_paths.size() == image::kCubemapImageLayer,
              std::format("Length of 'relative_paths' ({}) is not 6",
                          relative_paths.size()));
  return LoadCubemapFromFiles(GetFullPaths(directory, relative_paths), flip_y);
}

Image Image::LoadImagesFromMemory(const Dimension& dimension,
                                  std::span<const void* const> raw_data_ptrs,
                                  bool flip_y) {
  const int num_layers = raw_data_ptrs.size();
  Type type;
  switch (num_layers) {
    case image::kSingleImageLayer:
      type = Type::kSingle;
      break;
    case image::kCubemapImageLayer:
      type = Type::kCubemap;
      break;
    default:
      FATAL("Unreachable");
  }

  const int channel = dimension.channel;
  ASSERT_TRUE(
      channel == image::kBwImageChannel || channel == image::kRgbaImageChannel,
      std::format("Unsupported number of channels: {}", channel));

  RawChunkedData data{dimension.size_per_layer(), num_layers};
  for (int layer = 0; layer < num_layers; ++layer) {
    const auto* raw_data = static_cast<const char*>(raw_data_ptrs[layer]);
    char* copied_data = data.GetMutData(layer);
    if (flip_y) {
      const size_t stride = dimension.width * channel;
      for (int row = 0; row < dimension.height; ++row) {
        std::memcpy(copied_data + stride * row,
                    raw_data + stride * (dimension.height - row - 1), stride);
      }
    } else {
      std::memcpy(copied_data, raw_data, data.chunk_size());
    }
  }

  return {type, dimension, std::move(data)};
}

std::vector<const void*> Image::GetDataPtrs() const {
  std::vector<const void*> data_ptrs(GetNumLayers());
  for (int layer = 0; layer < data_ptrs.size(); ++layer) {
    data_ptrs[layer] = data_.GetData(layer);
  }
  return data_ptrs;
}

}  // namespace lighter::base
