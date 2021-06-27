//
//  image.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image.h"

#include <optional>
#include <vector>

#include "lighter/renderer/vk/buffer_util.h"
#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "third_party/absl/container/flat_hash_set.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

using ir::ImageUsage;
using ir::MultisamplingMode;

// Extracts width and height from 'dimension'.
intl::Extent2D ExtractExtent(const common::Image::Dimension& dimension) {
  return util::CreateExtent(dimension.width, dimension.height);
}

// Returns the first image format among 'candidates' that has the specified
// 'features'. If not found, returns std::nullopt.
std::optional<intl::Format> FindImageFormatWithFeature(
    const Context& context, absl::Span<const intl::Format> candidates,
    intl::FormatFeatureFlags features) {
  for (const intl::Format format : candidates) {
    const intl::FormatProperties properties =
        context.physical_device()->getFormatProperties(format);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return std::nullopt;
}

intl::Format ChooseColorImageFormat(const Context& context, int channel,
                                    bool high_precision,
                                    absl::Span<const ImageUsage> usages) {
  switch (channel) {
    case common::image::kBwImageChannel: {
      const intl::Format best_format = high_precision ? intl::Format::eR16Sfloat
                                                      : intl::Format::eR8Unorm;
      if (!ImageUsage::IsLinearAccessed(usages)) {
        return best_format;
      }

      // VK_FORMAT_R8_UNORM and VK_FORMAT_R16_SFLOAT have mandatory support for
      // sampling, but may not support linear access. We switch to 4-channel
      // formats instead since they have mandatory support for both.
      if (FindImageFormatWithFeature(
              context, {best_format},
              intl::FormatFeatureFlagBits::eStorageImage).has_value()) {
        return best_format;
      } else {
#ifndef NDEBUG
        LOG_INFO << "The single channel image format does not support linear "
                    "access, use the 4-channel format instead";
#endif  // !NDEBUG
        return high_precision ? intl::Format::eR16G16B16A16Sfloat
                              : intl::Format::eR8G8B8A8Unorm;
      }
    }

    case common::image::kRgbaImageChannel:
      return high_precision ? intl::Format::eR16G16B16A16Sfloat
                            : intl::Format::eR8G8B8A8Unorm;

    default:
      FATAL(absl::StrFormat(
          "Number of channels can only be 1 or 4, while %d provided", channel));
  }
}

intl::Format ChooseDepthStencilImageFormat(const Context& context) {
  const auto format = FindImageFormatWithFeature(
      context, {intl::Format::eD24UnormS8Uint, intl::Format::eD32SfloatS8Uint},
      intl::FormatFeatureFlagBits::eDepthStencilAttachment);
  ASSERT_HAS_VALUE(format, "Failed to find depth stencil image format");
  return format.value();
}

// Creates an image.
intl::Image CreateImage(
    const Context& context, intl::ImageCreateFlags create_flags,
    intl::Format format, const intl::Extent2D& extent, uint32_t mip_levels,
    uint32_t layer_count, intl::SampleCountFlagBits sample_count,
    intl::ImageUsageFlags usage_flags,
    const std::vector<uint32_t>& unique_queue_family_indices) {
  const auto image_create_info = intl::ImageCreateInfo{}
      .setFlags(create_flags)
      .setImageType(intl::ImageType::e2D)
      .setFormat(format)
      .setExtent({extent.width, extent.height, /*depth=*/1})
      .setMipLevels(mip_levels)
      .setArrayLayers(layer_count)
      .setSamples(sample_count)
      .setTiling(intl::ImageTiling::eOptimal)
      .setUsage(usage_flags)
      .setSharingMode(intl::SharingMode::eExclusive)
      .setQueueFamilyIndices(unique_queue_family_indices)
      .setInitialLayout(intl::ImageLayout::eUndefined);
  return context.device()->createImage(image_create_info,
                                       *context.host_allocator());
}

// Allocates device memory for 'image' with 'memory_properties'.
intl::DeviceMemory CreateImageMemory(const Context& context, intl::Image image,
                                     intl::MemoryPropertyFlags property_flags) {
  const intl::Device device = *context.device();
  const auto requirements = device.getImageMemoryRequirements(image);
  intl::DeviceMemory device_memory = buffer::CreateDeviceMemory(
      context, requirements, property_flags);
  // Bind the allocated memory with 'image'. If this memory is used for
  // multiple images, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  device.bindImageMemory(image, device_memory, /*memoryOffset=*/0);
  return device_memory;
}

}  // namespace

std::unique_ptr<DeviceImage> GeneralDeviceImage::CreateColorImage(
    SharedContext context, std::string_view name,
    const common::Image::Dimension& dimension,
    MultisamplingMode multisampling_mode, bool high_precision,
    absl::Span<const ImageUsage> usages) {
  const intl::Format format = ChooseColorImageFormat(
      *context, dimension.channel, high_precision, usages);
  return absl::WrapUnique(new GeneralDeviceImage(
      std::move(context), name, format, ExtractExtent(dimension),
      kSingleMipLevel, CAST_TO_UINT(dimension.layer), multisampling_mode,
      usages));
}

std::unique_ptr<DeviceImage> GeneralDeviceImage::CreateColorImage(
    SharedContext context, std::string_view name, const common::Image& image,
    bool generate_mipmaps, absl::Span<const ImageUsage> usages) {
  const auto& dimension = image.dimension();
  const intl::Format format = ChooseColorImageFormat(
      *context, dimension.channel, /*high_precision=*/false, usages);
  // TODO: Generate mipmaps and change mip_levels.
  return absl::WrapUnique(new GeneralDeviceImage(
      std::move(context), name, format, ExtractExtent(dimension),
      kSingleMipLevel, CAST_TO_UINT(dimension.layer), MultisamplingMode::kNone,
      usages));
}

std::unique_ptr<DeviceImage> GeneralDeviceImage::CreateDepthStencilImage(
    SharedContext context, std::string_view name, const intl::Extent2D& extent,
    MultisamplingMode multisampling_mode, absl::Span<const ImageUsage> usages) {
  const intl::Format format = ChooseDepthStencilImageFormat(*context);
  return absl::WrapUnique(new GeneralDeviceImage(
      std::move(context), name, format, extent, kSingleMipLevel,
      kSingleImageLayer, multisampling_mode, usages));
}

GeneralDeviceImage::GeneralDeviceImage(
    SharedContext context, std::string_view name, intl::Format format,
    const intl::Extent2D& extent, uint32_t mip_levels, uint32_t layer_count,
    MultisamplingMode multisampling_mode, absl::Span<const ImageUsage> usages)
    : DeviceImage{
          name, format, FATAL_IF_NULL(context)->physical_device().sample_count(
                            multisampling_mode)},
      context_{std::move(context)} {
  intl::ImageCreateFlags create_flags;
  if (layer_count == kCubemapImageLayer) {
    create_flags |= intl::ImageCreateFlagBits::eCubeCompatible;
  } else {
    ASSERT_TRUE(layer_count == kSingleImageLayer,
                absl::StrFormat("Unsupported layer count: %d", layer_count));
  }

  absl::flat_hash_set<uint32_t> queue_family_indices_set;
  for (const ImageUsage& usage : usages) {
    const auto queue_family_index =
        image::GetQueueFamilyIndex(*context_, usage);
    if (queue_family_index.has_value()) {
      queue_family_indices_set.insert(queue_family_index.value());
    }
  }
  ASSERT_NON_EMPTY(queue_family_indices_set,
                   "Cannot find any queue used for this image");
  const std::vector<uint32_t> unique_queue_family_indices{
      queue_family_indices_set.begin(),
      queue_family_indices_set.end(),
  };

  image_ = CreateImage(
      *context_, create_flags, format, extent, mip_levels, layer_count,
      sample_count(), image::GetImageUsageFlags(usages),
      unique_queue_family_indices);
  device_memory_ = CreateImageMemory(
      *context_, image_, intl::MemoryPropertyFlagBits::eDeviceLocal);
}

GeneralDeviceImage::~GeneralDeviceImage() {
  context_->device()->destroy(image_, *context_->host_allocator());
  buffer::FreeDeviceMemory(*context_, device_memory_);
}

}  // namespace lighter::renderer::vk
