//
//  image.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_H
#define LIGHTER_RENDERER_VK_IMAGE_H

#include "lighter/common/image.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/context.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace vk {

class DeviceImage : public renderer::DeviceImage {
 public:
  DeviceImage(SharedContext context, const common::Image& image,
              bool generate_mipmaps, absl::Span<const ImageUsage> usages);

  DeviceImage(SharedContext context, const common::Image::Dimension& dimension,
              MultisamplingMode multisampling_mode,
              absl::Span<const ImageUsage> usages);

  // This class is neither copyable nor movable.
  DeviceImage(const DeviceImage&) = delete;
  DeviceImage& operator=(const DeviceImage&) = delete;

 private:
  const SharedContext context_;
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
