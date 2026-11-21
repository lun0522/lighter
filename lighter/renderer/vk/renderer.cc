//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

namespace lighter::renderer::vk {

Renderer::Renderer(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs)
    : WithSharedContext{Context::CreateContext(
          application_name, debug_message_config, window_ptrs)},
      ir::Renderer{std::move(window_ptrs)} {
  swapchains_.resize(num_windows());
  for (int i = 0; i < num_windows(); ++i) {
    RecreateSwapchain(/*window_index=*/i);
  }
}

void Renderer::RecreateSwapchain(int window_index) {
  const auto& window = *windows().at(window_index);
  swapchains_[window_index] =
      std::make_unique<Swapchain>(context_, window_index, window);
}

std::unique_ptr<ir::RenderPass> Renderer::CreateRenderPass(
    ir::RenderPassDescriptor&& /*descriptor*/) const {
  class DynamicRenderPass : public ir::RenderPass {};
  return std::make_unique<DynamicRenderPass>();
}

}  // namespace lighter::renderer::vk
