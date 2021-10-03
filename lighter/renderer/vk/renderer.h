//
//  renderer.h
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDERER_H
#define LIGHTER_RENDERER_VK_RENDERER_H

#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/pipeline.h"
#include "lighter/renderer/vk/render_pass.h"
#include "lighter/renderer/vk/swapchain.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk {

class Renderer : public WithSharedContext,
                 public ir::Renderer {
 public:
  Renderer(const char* application_name,
           const std::optional<ir::debug_message::Config>& debug_message_config,
           std::vector<const common::Window*>&& window_ptrs);

  Renderer(const char* application_name,
           const std::optional<ir::debug_message::Config>& debug_message_config,
           absl::Span<const common::Window* const> windows)
      : Renderer{application_name, debug_message_config,
                 {windows.begin(), windows.end()}} {}

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  // TODO: Should handle this internally.
  void RecreateSwapchain(int window_index);

  // Buffer

  std::unique_ptr<ir::Buffer> CreateBuffer(
      Buffer::UpdateRate update_rate, size_t initial_size,
      absl::Span<const ir::BufferUsage> usages) const override {
    return std::make_unique<Buffer>(context_, update_rate, initial_size,
                                    usages);
  }

  // Image

  const Image& GetSwapchainImage(int window_index) const override {
    return swapchains_.at(window_index)->image();
  }

  std::unique_ptr<ir::Image> CreateColorImage(
      std::string_view name, const common::Image::Dimension& dimension,
      ir::MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ir::ImageUsage> usages) const override {
    return SingleImage::CreateColorImage(
        context_, name, dimension, multisampling_mode, high_precision, usages);
  }

  std::unique_ptr<ir::Image> CreateColorImage(
      std::string_view name, const common::Image& image, bool generate_mipmaps,
      absl::Span<const ir::ImageUsage> usages) const override {
    return SingleImage::CreateColorImage(context_, name, image,
                                         generate_mipmaps, usages);
  }

  std::unique_ptr<ir::Image> CreateDepthStencilImage(
      std::string_view name, const glm::ivec2& extent,
      ir::MultisamplingMode multisampling_mode,
      absl::Span<const ir::ImageUsage> usages) const override {
    return SingleImage::CreateDepthStencilImage(
        context_, name, extent, multisampling_mode, usages);
  }

  // Pass

  std::unique_ptr<ir::RenderPass> CreateRenderPass(
      ir::RenderPassDescriptor&& descriptor) const override {
    return std::make_unique<RenderPass>(context_, std::move(descriptor));
  }

  std::unique_ptr<ir::ComputePass> CreateComputePass(
      ir::ComputePassDescriptor&& descriptor) const override {
    FATAL("Not implemented yet");
  }

 private:
  std::vector<std::unique_ptr<Swapchain>> swapchains_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_RENDERER_H
