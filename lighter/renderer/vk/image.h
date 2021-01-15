//
//  image.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_H
#define LIGHTER_RENDERER_VK_IMAGE_H

#include <memory>
#include <string_view>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk {

class DeviceImage : public renderer::DeviceImage {
 public:
  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

  static const DeviceImage& Cast(const renderer::DeviceImage& image) {
    return dynamic_cast<const DeviceImage&>(image);
  }

  // Accessors.
  VkFormat format() const { return format_; }
  VkSampleCountFlagBits sample_count() const { return sample_count_; }

 protected:
  DeviceImage(std::string_view name, VkFormat format,
              VkSampleCountFlagBits sample_count)
      : renderer::DeviceImage{name},
        format_{format}, sample_count_{sample_count} {}

 private:
  const VkFormat format_;

  const VkSampleCountFlagBits sample_count_;
};

class GeneralDeviceImage : public DeviceImage {
 public:
  static std::unique_ptr<DeviceImage> CreateColorImage(
      SharedContext context, std::string_view name,
      const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ImageUsage> usages);

  static std::unique_ptr<DeviceImage> CreateColorImage(
      SharedContext context, std::string_view name, const common::Image& image,
      bool generate_mipmaps, absl::Span<const ImageUsage> usages);

  static std::unique_ptr<DeviceImage> CreateDepthStencilImage(
      SharedContext context, std::string_view name, const VkExtent2D& extent,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages);

  // This class is neither copyable nor movable.
  GeneralDeviceImage(const GeneralDeviceImage&) = delete;
  GeneralDeviceImage& operator=(const GeneralDeviceImage&) = delete;

  ~GeneralDeviceImage() override;

 private:
  GeneralDeviceImage(SharedContext context, std::string_view name,
                     VkFormat format, const VkExtent2D& extent,
                     uint32_t mip_levels, uint32_t layer_count,
                     MultisamplingMode multisampling_mode,
                     absl::Span<const ImageUsage> usages);

  const SharedContext context_;

  // Opaque image object.
  VkImage image_;

  // TODO: Hold multiple images in one block of device memory.
  // Opaque device memory object.
  VkDeviceMemory device_memory_;
};

class SwapchainImage : public DeviceImage {
 public:
  SwapchainImage(std::string_view name, std::vector<VkImage>&& images,
                 VkFormat format)
      : DeviceImage{name, format, VK_SAMPLE_COUNT_1_BIT},
        images_{std::move(images)} {}

  // This class is neither copyable nor movable.
  SwapchainImage(const SwapchainImage&) = delete;
  SwapchainImage& operator=(const SwapchainImage&) = delete;

 private:
  // Opaque image objects.
  std::vector<VkImage> images_;
};

class SampledImageView : public renderer::SampledImageView {
 public:
  SampledImageView(const renderer::DeviceImage& image,
                   const SamplerDescriptor& sampler_descriptor) {}

  // This class provides copy constructor and move constructor.
  SampledImageView(SampledImageView&&) noexcept = default;
  SampledImageView(const SampledImageView&) = default;
};

}  // namespace vk::renderer::lighter

#endif  // LIGHTER_RENDERER_VK_IMAGE_H
