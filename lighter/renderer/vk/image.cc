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
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter::renderer::vk {
namespace {

using ir::ImageUsage;
using ir::MultisamplingMode;

const int kSingleMipLevel = common::image::kSingleMipLevel;

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
#endif  // DEBUG
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

const std::vector<intl::Format>& GetSupportedDepthStencilFormats() {
  static const auto* formats = new std::vector<intl::Format>{
      intl::Format::eD24UnormS8Uint, intl::Format::eD32SfloatS8Uint};
  return *formats;
}

intl::Format ChooseDepthStencilImageFormat(const Context& context) {
  const auto format = FindImageFormatWithFeature(
      context, GetSupportedDepthStencilFormats(),
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

intl::ImageViewType Image::GetViewType() const {
  switch (layer_type()) {
    case LayerType::kSingle:
      return intl::ImageViewType::e2D;
    case LayerType::kCubemap:
      return intl::ImageViewType::eCube;
  }
}

intl::ImageAspectFlags Image::GetAspectFlags() const {
  const auto formats_span = absl::MakeSpan(GetSupportedDepthStencilFormats());
  if (common::util::Contains(formats_span, format())) {
    return intl::ImageAspectFlagBits::eDepth |
           intl::ImageAspectFlagBits::eStencil;
  } else {
    return intl::ImageAspectFlagBits::eColor;
  }
}

std::unique_ptr<SingleImage> SingleImage::CreateColorImage(
    const SharedContext& context, std::string_view name,
    const common::Image::Dimension& dimension,
    MultisamplingMode multisampling_mode, bool high_precision,
    absl::Span<const ImageUsage> usages) {
  const intl::Format format = ChooseColorImageFormat(
      *context, dimension.channel, high_precision, usages);
  return absl::WrapUnique(new SingleImage(
      context, name, LayerType::kSingle, dimension.extent(), kSingleMipLevel,
      format, multisampling_mode, usages));
}

std::unique_ptr<SingleImage> SingleImage::CreateColorImage(
    const SharedContext& context, std::string_view name,
    const common::Image& image, bool generate_mipmaps,
    absl::Span<const ImageUsage> usages) {
  const intl::Format format = ChooseColorImageFormat(
      *context, image.channel(), /*high_precision=*/false, usages);
  // TODO: Generate mipmaps and change mip_levels.
  return absl::WrapUnique(new SingleImage(
      context, name, image.type(), image.extent(), kSingleMipLevel, format,
      MultisamplingMode::kNone, usages));
}

std::unique_ptr<SingleImage> SingleImage::CreateDepthStencilImage(
    const SharedContext& context, std::string_view name,
    const glm::ivec2& extent, MultisamplingMode multisampling_mode,
    absl::Span<const ImageUsage> usages) {
  const intl::Format format = ChooseDepthStencilImageFormat(*context);
  return absl::WrapUnique(new SingleImage(
      context, name, LayerType::kSingle, extent, kSingleMipLevel, format,
      multisampling_mode, usages));
}

SingleImage::SingleImage(
    const SharedContext& context, std::string_view name, LayerType layer_type,
    const glm::ivec2& extent, int mip_levels, intl::Format format,
    ir::MultisamplingMode multisampling_mode,
    absl::Span<const ir::ImageUsage> usages)
    : WithSharedContext{context},
      Image{Type::kSingle, name, layer_type, extent, mip_levels, format,
            context_->physical_device().sample_count(multisampling_mode)} {
  intl::ImageCreateFlags create_flags;
  if (layer_type == LayerType::kCubemap) {
    create_flags |= intl::ImageCreateFlagBits::eCubeCompatible;
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
      *context_, create_flags, format, util::ToExtent(extent), mip_levels,
      CAST_TO_UINT(GetNumLayers()), sample_count(),
      image::GetImageUsageFlags(usages), unique_queue_family_indices);
  device_memory_ = CreateImageMemory(
      *context_, image_, intl::MemoryPropertyFlagBits::eDeviceLocal);
}

SingleImage::~SingleImage() {
  context_->DeviceDestroy(image_);
  buffer::FreeDeviceMemory(*context_, device_memory_);
}

}  // namespace lighter::renderer::vk
