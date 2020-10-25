//
//  image.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/image.h"

#include <algorithm>
#include <vector>

#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/memory/memory.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace {

// Extracts width and height from 'dimension'.
VkExtent2D ExtractExtent(const common::Image::Dimension& dimension) {
  return util::CreateExtent(dimension.width, dimension.height);
}

// Returns the first image format among 'candidates' that has the specified
// 'features'. If not found, returns absl::nullopt.
absl::optional<VkFormat> FindImageFormatWithFeature(
    const Context& context, absl::Span<const VkFormat> candidates,
    VkFormatFeatureFlags features) {
  for (auto format : candidates) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(*context.physical_device(),
                                        format, &properties);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return absl::nullopt;
}

VkFormat ChooseColorImageFormat(const Context& context, int channel,
                                absl::Span<const ImageUsage> usages) {
  switch (channel) {
    case common::image::kBwImageChannel: {
      const bool use_high_precision = ImageUsage::UseHighPrecision(usages);
      const VkFormat best_format =
          use_high_precision ? VK_FORMAT_R16_SFLOAT : VK_FORMAT_R8_UNORM;
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
#endif /* !NDEBUG */
        return use_high_precision ? VK_FORMAT_R16G16B16A16_SFLOAT
                                  : VK_FORMAT_R8G8B8A8_UNORM;
      }
    }

    case common::image::kRgbaImageChannel:
      if (ImageUsage::UseHighPrecision(usages)) {
        return VK_FORMAT_R16G16B16A16_SFLOAT;
      } else {
        return VK_FORMAT_R8G8B8A8_UNORM;
      }

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

// Returns the maximum number of samples per pixel indicated by 'sample_counts'.
VkSampleCountFlagBits GetMaxSampleCount(VkSampleCountFlags sample_counts) {
  for (auto count : {VK_SAMPLE_COUNT_64_BIT,
                     VK_SAMPLE_COUNT_32_BIT,
                     VK_SAMPLE_COUNT_16_BIT,
                     VK_SAMPLE_COUNT_8_BIT,
                     VK_SAMPLE_COUNT_4_BIT,
                     VK_SAMPLE_COUNT_2_BIT}) {
    if (sample_counts & count) {
      return count;
    }
  }
  FATAL("Multisampling is not supported by hardware");
}

// Returns the number of samples per pixel chosen according to 'mode' and
// physical device limits.
VkSampleCountFlagBits ChooseSampleCount(const Context& context,
                                        MultisamplingMode mode) {
  if (mode == MultisamplingMode::kNone) {
    return VK_SAMPLE_COUNT_1_BIT;
  }

  const VkPhysicalDeviceLimits& limits = context.physical_device().limits();
  const VkSampleCountFlags sample_counts = std::min({
      limits.framebufferColorSampleCounts,
      limits.framebufferDepthSampleCounts,
      limits.framebufferStencilSampleCounts,
  });
  const VkSampleCountFlagBits max_sample_count =
      GetMaxSampleCount(sample_counts);

  switch (mode) {
    case MultisamplingMode::kNone:
      FATAL("Not reachable");

    case MultisamplingMode::kDecent:
      return std::min(VK_SAMPLE_COUNT_4_BIT, max_sample_count);

    case MultisamplingMode::kBest:
      return max_sample_count;
  }
}

// Creates an image that can be used by the graphics queue.
VkImage CreateImage(const Context& context, VkImageCreateFlags create_flags,
                    VkFormat format, const VkExtent2D& extent,
                    uint32_t mip_levels, uint32_t layer_count,
                    VkSampleCountFlagBits sample_count,
                    VkImageUsageFlags usage_flags,
                    const util::QueueUsage& queue_usage) {
  const std::vector<uint32_t> unique_queue_family_indices =
      queue_usage.GetUniqueQueueFamilyIndices();
  const VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      create_flags,
      VK_IMAGE_TYPE_2D,
      format,
      {extent.width, extent.height, /*depth=*/1},
      mip_levels,
      layer_count,
      sample_count,
      VK_IMAGE_TILING_OPTIMAL,
      usage_flags,
      queue_usage.sharing_mode(),
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

} /* namespace */

std::unique_ptr<DeviceImage> DeviceImage::CreateColorImage(
    SharedContext context, const common::Image::Dimension& dimension,
    MultisamplingMode multisampling_mode,
    absl::Span<const ImageUsage> usages) {
  const VkFormat format =
      ChooseColorImageFormat(*context, dimension.channel, usages);
  return absl::make_unique<DeviceImage>(
      std::move(context), format, ExtractExtent(dimension), kSingleMipLevel,
      static_cast<uint32_t>(dimension.layer), multisampling_mode, usages);
}

std::unique_ptr<DeviceImage> DeviceImage::CreateColorImage(
    SharedContext context, const common::Image& image, bool generate_mipmaps,
    absl::Span<const ImageUsage> usages) {
  const auto& dimension = image.dimension();
  const VkFormat format =
      ChooseColorImageFormat(*context, dimension.channel, usages);
  // TODO: Generate mipmaps and change mip_levels.
  return absl::make_unique<DeviceImage>(
      std::move(context), format, ExtractExtent(dimension), kSingleMipLevel,
      static_cast<uint32_t>(dimension.layer), MultisamplingMode::kNone, usages);
}

std::unique_ptr<DeviceImage> DeviceImage::CreateDepthStencilImage(
    SharedContext context, const VkExtent2D& extent,
    MultisamplingMode multisampling_mode,
    absl::Span<const ImageUsage> usages) {
  const VkFormat format = ChooseDepthStencilImageFormat(*context);
  return absl::make_unique<DeviceImage>(
      std::move(context), format, extent, kSingleMipLevel, kSingleImageLayer,
      multisampling_mode, usages);
}

DeviceImage::DeviceImage(SharedContext context, VkFormat format,
                         const VkExtent2D& extent, uint32_t mip_levels,
                         uint32_t layer_count,
                         MultisamplingMode multisampling_mode,
                         absl::Span<const ImageUsage> usages)
    : context_{std::move(FATAL_IF_NULL(context))} {
  VkImageCreateFlags create_flags = nullflag;
  if (layer_count == kCubemapImageLayer) {
    create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  } else {
    ASSERT_TRUE(layer_count == kSingleImageLayer,
                absl::StrFormat("Unsupported layer count: %d", layer_count));
  }

  util::QueueUsage queue_usage;
  for (const ImageUsage& usage : usages) {
    const auto queue_family_index =
        image::GetQueueFamilyIndex(*context_, usage);
    if (queue_family_index.has_value()) {
      queue_usage.AddQueueFamily(queue_family_index.value());
    }
  }
  ASSERT_NON_EMPTY(queue_usage.unique_family_indices_set(),
                   "Cannot find any queue used for this image");

  image_ = CreateImage(
      *context_, create_flags, format, extent, mip_levels, layer_count,
      ChooseSampleCount(*context_, multisampling_mode),
      image::GetImageUsageFlags(usages), queue_usage);
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
