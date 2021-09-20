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
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter::renderer::vk {

class DeviceImage : public ir::DeviceImage {
 public:
  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

  static const DeviceImage& Cast(const ir::DeviceImage& image) {
    return dynamic_cast<const DeviceImage&>(image);
  }

  // Accessors.
  intl::Extent2D extent() const {
    return util::CreateExtent(width(), height());
  }
  intl::Format format() const { return format_; }
  intl::SampleCountFlagBits sample_count() const { return sample_count_; }

 protected:
  DeviceImage(std::string_view name, Type type, const glm::ivec2& extent,
              int mip_levels, intl::Format format,
              intl::SampleCountFlagBits sample_count)
      : ir::DeviceImage{name, type, extent, mip_levels},
        format_{format}, sample_count_{sample_count} {}

 private:
  const intl::Format format_;

  const intl::SampleCountFlagBits sample_count_;
};

class GeneralDeviceImage : public WithSharedContext,
                           public DeviceImage {
 public:
  static std::unique_ptr<DeviceImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const common::Image::Dimension& dimension,
      ir::MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ir::ImageUsage> usages);

  static std::unique_ptr<DeviceImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const common::Image& image, bool generate_mipmaps,
      absl::Span<const ir::ImageUsage> usages);

  static std::unique_ptr<DeviceImage> CreateDepthStencilImage(
      const SharedContext& context, std::string_view name,
      const glm::ivec2& extent, ir::MultisamplingMode multisampling_mode,
      absl::Span<const ir::ImageUsage> usages);

  // This class is neither copyable nor movable.
  GeneralDeviceImage(const GeneralDeviceImage&) = delete;
  GeneralDeviceImage& operator=(const GeneralDeviceImage&) = delete;

  ~GeneralDeviceImage() override;

 private:
  GeneralDeviceImage(
    const SharedContext& context, std::string_view name, Type type,
    const glm::ivec2& extent, int mip_levels, intl::Format format,
    ir::MultisamplingMode multisampling_mode,
    absl::Span<const ir::ImageUsage> usages);

  // Opaque image object.
  intl::Image image_;

  // TODO: Integrate VMA.
  // Opaque device memory object.
  intl::DeviceMemory device_memory_;
};

class SwapchainImage : public DeviceImage {
 public:
  SwapchainImage(std::string_view name, std::vector<intl::Image>&& images,
                 const glm::ivec2& extent, intl::Format format)
      : DeviceImage{name, Type::kSingle, extent, /*mip_levels=*/1,
                    format, intl::SampleCountFlagBits::e1},
        images_{std::move(images)} {}

  // This class is neither copyable nor movable.
  SwapchainImage(const SwapchainImage&) = delete;
  SwapchainImage& operator=(const SwapchainImage&) = delete;

 private:
  // Opaque image objects.
  std::vector<intl::Image> images_;
};

class SampledImageView : public ir::SampledImageView {
 public:
  SampledImageView(const ir::DeviceImage& image,
                   const ir::SamplerDescriptor& sampler_descriptor) {}

  // This class provides copy constructor and move constructor.
  SampledImageView(SampledImageView&&) noexcept = default;
  SampledImageView(const SampledImageView&) = default;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_IMAGE_H
