//
//  renderer.h
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDERER_H
#define LIGHTER_RENDERER_VK_RENDERER_H

#include <memory>
#include <vector>

#include "lighter/common/image.h"
#include "lighter/renderer/buffer_usage.h"
#include "lighter/renderer/renderer.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/pipeline.h"
#include "lighter/renderer/vk/swapchain.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

class Renderer : public renderer::Renderer {
 public:
  Renderer(absl::string_view application_name,
           const absl::optional<debug_message::Config>& debug_message_config,
           std::vector<WindowConfig>&& window_configs);

  Renderer(absl::string_view application_name,
           const absl::optional<debug_message::Config>& debug_message_config,
           absl::Span<const WindowConfig> window_configs)
      : Renderer{application_name, debug_message_config,
                 {window_configs.begin(), window_configs.end()}} {}

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  void RecreateSwapchain(int window_index);

  /* Buffer */

  std::unique_ptr<renderer::DeviceBuffer> CreateDeviceBuffer(
      DeviceBuffer::UpdateRate update_rate, size_t initial_size,
      absl::Span<const BufferUsage> usages) const override {
    return absl::make_unique<DeviceBuffer>(context_, update_rate, initial_size,
                                           usages);
  }

  /* Image */

  std::unique_ptr<renderer::DeviceImage> CreateColorImage(
      const common::Image::Dimension& dimension,
      MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const override {
    return DeviceImage::CreateColorImage(context_, dimension,
                                         multisampling_mode, usages);
  }

  std::unique_ptr<renderer::DeviceImage> CreateColorImage(
      const common::Image& image, bool generate_mipmaps,
      absl::Span<const ImageUsage> usages) const override {
    return DeviceImage::CreateColorImage(context_, image, generate_mipmaps,
                                         usages);
  }

  std::unique_ptr<renderer::DeviceImage> CreateDepthStencilImage(
      int width, int height, MultisamplingMode multisampling_mode,
      absl::Span<const ImageUsage> usages) const override {
    const VkExtent2D extent = util::CreateExtent(width, height);
    return DeviceImage::CreateDepthStencilImage(context_, extent,
                                                multisampling_mode, usages);
  }

  /* Pipeline */

  std::unique_ptr<renderer::Pipeline> CreateGraphicsPipeline(
      const GraphicsPipelineDescriptor& descriptor) const override {
    return absl::make_unique<Pipeline>(context_, descriptor);
  }

  std::unique_ptr<renderer::Pipeline> CreateComputePipeline(
      const ComputePipelineDescriptor& descriptor) const override {
    return absl::make_unique<Pipeline>(context_, descriptor);
  }

  /* Pass */

  std::unique_ptr<GraphicsPass> CreateGraphicsPass(
      const GraphicsPassDescriptor& descriptor) const override {
    FATAL("Not implemented yet");
  }

  std::unique_ptr<ComputePass> CreateComputePass(
      const ComputePassDescriptor& descriptor) const override {
    FATAL("Not implemented yet");
  }

 private:
  const SharedContext context_;

  std::vector<std::unique_ptr<Swapchain>> swapchains_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_RENDERER_H */
