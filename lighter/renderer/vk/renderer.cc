//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

namespace lighter::renderer::vk {

Renderer::Renderer(
    std::string_view application_name,
    const std::optional<debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs)
    : renderer::Renderer{std::move(window_ptrs)},
      context_{Context::CreateContext(application_name, debug_message_config,
                                      windows(),
                                      Swapchain::GetRequiredExtensions())} {
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

}  // namespace vk::renderer::lighter
