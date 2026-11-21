//
//  renderer.h
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDERER_H
#define LIGHTER_RENDERER_VK_RENDERER_H

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/base/image.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/buffer.h"
#include "lighter/renderer/vk/command.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/image.h"
#include "lighter/renderer/vk/render_graph.h"
#include "lighter/renderer/vk/swapchain.h"

namespace lighter::renderer::vk {

// Configures process environment variables for Vulkan/MoltenVK. This should be
// called once before creating any windows or Vulkan instances.
void InitVulkan();

// Vulkan implementation of ir::Renderer managing swapchains, per-frame command
// submission, and compiled RenderGraph execution.
class Renderer : public WithSharedContext, public ir::Renderer {
 public:
  Renderer(const char* application_name,
           const std::optional<ir::debug_message::Config>& debug_message_config,
           std::vector<base::Window*>&& window_ptrs);

  Renderer(const char* application_name,
           const std::optional<ir::debug_message::Config>& debug_message_config,
           std::span<base::Window* const> windows)
      : Renderer{application_name, debug_message_config,
                 std::vector<base::Window*>{windows.begin(), windows.end()}} {}

  // This class is neither copyable nor movable.
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  // Buffer

  std::unique_ptr<ir::Buffer> CreateBuffer(
      Buffer::UpdateRate update_rate, size_t initial_size,
      std::span<const ir::BufferUsage> usages) const override {
    return std::make_unique<Buffer>(context_, update_rate, initial_size,
                                    usages);
  }

  // Image

  std::unique_ptr<ir::Image> CreateColorImage(
      std::string_view name, const base::Image::Dimension& dimension,
      ir::MultisamplingMode multisampling_mode, bool high_precision,
      std::span<const ir::ImageUsage> usages) const override {
    return OwnedImage::CreateColorImage(
        context_, name, dimension, multisampling_mode, high_precision, usages);
  }

  std::unique_ptr<ir::Image> CreateColorImage(
      std::string_view name, const ir::Image& match_target,
      ir::MultisamplingMode multisampling_mode,
      std::span<const ir::ImageUsage> usages) const override {
    return OwnedImage::CreateColorImage(
        context_, name, Image::Cast(match_target), multisampling_mode, usages);
  }

  std::unique_ptr<ir::Image> CreateColorImage(
      std::string_view name, const base::Image& image, bool generate_mipmaps,
      std::span<const ir::ImageUsage> usages) const override {
    return OwnedImage::CreateColorImage(context_, name, image, generate_mipmaps,
                                        usages);
  }

  std::unique_ptr<ir::Image> CreateDepthStencilImage(
      std::string_view name, const glm::ivec2& extent,
      ir::MultisamplingMode multisampling_mode,
      std::span<const ir::ImageUsage> usages) const override {
    return OwnedImage::CreateDepthStencilImage(context_, name, extent,
                                               multisampling_mode, usages);
  }

  // Frame Graph execution

  bool RenderFrame(int window_index, ir::RenderGraph& graph) override;

  void ExecuteRenderGraph(ir::RenderGraph& graph) override;

  const std::string& last_render_graph_graphviz() const {
    return render_graph_executor_->last_graphviz();
  }

  void WaitIdle() override { context_->WaitIdle(); }

 private:
  void RecreateSwapchain(int window_index);

  std::vector<std::unique_ptr<Swapchain>> swapchains_;
  std::vector<std::unique_ptr<PerFrameCommand>> per_frame_commands_;
  std::unique_ptr<RenderGraphExecutor> render_graph_executor_;
  std::vector<int> current_frames_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_RENDERER_H
