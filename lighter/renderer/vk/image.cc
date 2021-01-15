//
//  image.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "lighter/renderer/vk/buffer_util.h"
#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/type_mapping.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/container/flat_hash_set.h"

namespace lighter::renderer::vk {
namespace {

// Extracts width and height from 'dimension'.
VkExtent2D ExtractExtent(const common::Image::Dimension& dimension) {
  return util::CreateExtent(dimension.width, dimension.height);
}

// Returns the first image format among 'candidates' that has the specified
// 'features'. If not found, returns std::nullopt.
std::optional<VkFormat> FindImageFormatWithFeature(
    const Context& context, absl::Span<const VkFormat> candidates,
    VkFormatFeatureFlags features) {
  for (const auto format : candidates) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(*context.physical_device(),
                                        format, &properties);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return std::nullopt;
}

VkFormat ChooseColorImageFormat(const Context& context, int channel,
                                bool high_precision,
                                absl::Span<const ImageUsage> usages) {
  switch (channel) {
    case common::image::kBwImageChannel: {
      const VkFormat best_format = high_precision ? VK_FORMAT_R16_SFLOAT
                                                  : VK_FORMAT_R8_UNORM;
      if (!ImageUsage::IsLinearAccessed(usages)) {
        return best_format;
      }

      // VK_FORMAT_R8_UNORM and VK_FORMAT_R16_SFLOAT have mandatory support for
      // sampling, but may not support linear access. We switch to 4-channel
      // formats instead since they have mandatory support for both.
      if (FindImageFormatWithFeature(
              context, {best_format},
              VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT).has_value()) {
        return best_format;
      } else {
#ifndef NDEBUG
        LOG_INFO << "The single channel image format does not support linear "
                    "access, use the 4-channel format instead";
#endif  // !NDEBUG
        return high_precision ? VK_FORMAT_R16G16B16A16_SFLOAT
                              : VK_FORMAT_R8G8B8A8_UNORM;
      }
    }

    case common::image::kRgbaImageChannel:
      return high_precision ? VK_FORMAT_R16G16B16A16_SFLOAT
                            : VK_FORMAT_R8G8B8A8_UNORM;

    default:
      FATAL(absl::StrFormat(
          "Number of channels can only be 1 or 4, while %d provided", channel));
  }
}

VkFormat ChooseDepthStencilImageFormat(const Context& context) {
  const auto format = FindImageFormatWithFeature(
      context, {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT},
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  ASSERT_HAS_VALUE(format, "Failed to find depth stencil image format");
  return format.value();
}

// Creates an image.
VkImage CreateImage(const Context& context, VkImageCreateFlags create_flags,
                    VkFormat format, const VkExtent2D& extent,
                    uint32_t mip_levels, uint32_t layer_count,
                    VkSampleCountFlagBits sample_count,
                    VkImageUsageFlags usage_flags,
                    absl::Span<const uint32_t> unique_queue_family_indices) {
  const VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      create_flags,
      VK_IMAGE_TYPE_2D,
      format,
      {extent.width, extent.height, .depth = 1},
      mip_levels,
      layer_count,
      sample_count,
      VK_IMAGE_TILING_OPTIMAL,
      usage_flags,
      VK_SHARING_MODE_EXCLUSIVE,
      CONTAINER_SIZE(unique_queue_family_indices),
      unique_queue_family_indices.data(),
      VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage image;
  ASSERT_SUCCESS(vkCreateImage(*context.device(), &image_info,
                               *context.host_allocator(), &image),
                 "Failed to create image");
  return image;
}

// Allocates device memory for 'image' with 'memory_properties'.
VkDeviceMemory CreateImageMemory(const Context& context, const VkImage& image,
                                 VkMemoryPropertyFlags property_flags) {
  const VkDevice& device = *context.device();
  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(device, image, &requirements);

  VkDeviceMemory device_memory = buffer::CreateDeviceMemory(
      context, requirements, property_flags);

  // Bind the allocated memory with 'image'. If this memory is used for
  // multiple images, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  vkBindImageMemory(device, image, device_memory, /*memoryOffset=*/0);
  return device_memory;
}

}  // namespace

std::unique_ptr<DeviceImage> GeneralDeviceImage::CreateColorImage(
    SharedContext context, std::string_view name,
    const common::Image::Dimension& dimension,
    MultisamplingMode multisampling_mode, bool high_precision,
    absl::Span<const ImageUsage> usages) {
  const VkFormat format = ChooseColorImageFormat(*context, dimension.channel,
                                                 high_precision, usages);
  return std::make_unique<GeneralDeviceImage>(
      std::move(context), name, format, ExtractExtent(dimension),
      kSingleMipLevel, CAST_TO_UINT(dimension.layer), multisampling_mode,
      usages);
}

std::unique_ptr<DeviceImage> GeneralDeviceImage::CreateColorImage(
    SharedContext context, std::string_view name, const common::Image& image,
    bool generate_mipmaps, absl::Span<const ImageUsage> usages) {
  const auto& dimension = image.dimension();
  const VkFormat format = ChooseColorImageFormat(
      *context, dimension.channel, /*high_precision=*/false, usages);
  // TODO: Generate mipmaps and change mip_levels.
  return std::make_unique<GeneralDeviceImage>(
      std::move(context), name, format, ExtractExtent(dimension),
      kSingleMipLevel, CAST_TO_UINT(dimension.layer), MultisamplingMode::kNone,
      usages);
}

std::unique_ptr<DeviceImage> GeneralDeviceImage::CreateDepthStencilImage(
    SharedContext context, std::string_view name, const VkExtent2D& extent,
    MultisamplingMode multisampling_mode,
    absl::Span<const ImageUsage> usages) {
  const VkFormat format = ChooseDepthStencilImageFormat(*context);
  return std::make_unique<GeneralDeviceImage>(
      std::move(context), name, format, extent, kSingleMipLevel,
      kSingleImageLayer, multisampling_mode, usages);
}

GeneralDeviceImage::GeneralDeviceImage(
    SharedContext context, std::string_view name, VkFormat format,
    const VkExtent2D& extent, uint32_t mip_levels, uint32_t layer_count,
    MultisamplingMode multisampling_mode, absl::Span<const ImageUsage> usages)
    : renderer::DeviceImage{
          name, FATAL_IF_NULL(context)->physical_device().sample_count(
                    multisampling_mode)},
      context_{std::move(context)} {
  VkImageCreateFlags create_flags = nullflag;
  if (layer_count == kCubemapImageLayer) {
    create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
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
      type::ConvertSampleCount(sample_count()),
      image::GetImageUsageFlags(usages), unique_queue_family_indices);
  device_memory_ = CreateImageMemory(*context_, image_,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

GeneralDeviceImage::~GeneralDeviceImage() {
  vkDestroyImage(*context_->device(), image_, *context_->host_allocator());
  buffer::FreeDeviceMemory(*context_, device_memory_);
}

}  // namespace vk::renderer::lighter
