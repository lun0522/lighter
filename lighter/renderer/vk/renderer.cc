//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

namespace lighter {
namespace renderer {
namespace vk {

Renderer::Renderer(
    absl::string_view application_name,
    const absl::optional<debug_message::Config>& debug_message_config,
    std::vector<WindowConfig>&& window_configs)
    : renderer::Renderer{std::move(window_configs)},
      context_{Context::CreateContext(application_name, debug_message_config,
                                      GetWindows(),
                                      Swapchain::GetRequiredExtensions())} {
  swapchains_.resize(Renderer::window_configs().size());
  for (int i = 0; i < Renderer::window_configs().size(); ++i) {
    RecreateSwapchain(/*window_index=*/i);
  }
}

void Renderer::RecreateSwapchain(int window_index) {
  const WindowConfig& config = window_configs().at(window_index);
  swapchains_.at(window_index) = absl::make_unique<Swapchain>(
      context_, window_index, *config.window, config.multisampling_mode);
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
