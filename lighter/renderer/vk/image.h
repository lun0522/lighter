//
//  image.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_H
#define LIGHTER_RENDERER_VK_IMAGE_H

#include <memory>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

class DeviceImage : public renderer::DeviceImage {
 public:
  static std::unique_ptr<DeviceImage> CreateColorImage(
      SharedContext context, const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages);

  static std::unique_ptr<DeviceImage> CreateColorImage(
      SharedContext context, const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages);

  static std::unique_ptr<DeviceImage> CreateDepthStencilImage(
      SharedContext context, const VkExtent2D& extent,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages);

  DeviceImage(SharedContext context, VkFormat format, const VkExtent2D& extent,
              uint32_t mip_levels, uint32_t layer_count,
              MultisamplingMode multisampling_mode,
              absl::Span<const ImageUsage> usages);

  DeviceImage(SharedContext context, const VkImage& image)
      : context_{std::move(FATAL_IF_NULL(context))}, image_{image} {}

  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

  ~DeviceImage();

 private:
  const SharedContext context_;

  // Opaque image object.
  VkImage image_;

  // TODO: Hold multiple images in one block of device memory.
  // Opaque device memory object.
  VkDeviceMemory device_memory_;
};

class SampledImageView : public renderer::SampledImageView {
 public:
  SampledImageView(const renderer::DeviceImage& image,
                   const SamplerDescriptor& sampler_descriptor) {}

  // This class provides copy constructor and move constructor.
  SampledImageView(SampledImageView&&) noexcept = default;
  SampledImageView(const SampledImageView&) = default;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_IMAGE_H */
