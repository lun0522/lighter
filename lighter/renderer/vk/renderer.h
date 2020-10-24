//
//  renderer.h
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDERER_H
#define LIGHTER_RENDERER_VK_RENDERER_H

#include <memory>

#include "lighter/common/image.h"
#include "lighter/common/window.h"
#include "lighter/renderer/renderer.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/swapchain.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {
namespace vk {

class Renderer : public renderer::Renderer {
 public:
  Renderer(absl::string_view application_name,
           const absl::optional<debug_message::Config>& debug_message_config,
           absl::Span<const common::Window* const> windows)
      : context_{Context::CreateContext(
            application_name, debug_message_config, windows,
            Swapchain::GetRequiredExtensions())} {}

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  /* Image */

  std::unique_ptr<renderer::DeviceImage> CreateDeviceImage(
      const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages) const override {
    return absl::make_unique<DeviceImage>(
        context_, image, generate_mipmaps, usages);
  }

  std::unique_ptr<renderer::DeviceImage> CreateDeviceImage(
      const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const override {
    return absl::make_unique<DeviceImage>(
        context_, dimension, multisampling_mode, usages);
  }

  std::unique_ptr<renderer::SampledImageView> CreateSampledImageView(
      const renderer::DeviceImage& image,
      const SamplerDescriptor& sampler_descriptor) const override {
    return absl::make_unique<SampledImageView>(image, sampler_descriptor);
  }

 private:
  const SharedContext context_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_RENDERER_H */
