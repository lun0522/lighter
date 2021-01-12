//
//  context.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/context.h"

namespace lighter::renderer::vk {

Context::Context(
    std::string_view application_name,
    const std::optional<debug_message::Config>& debug_message_config,
    absl::Span<const common::Window* const> windows,
    absl::Span<const char* const> swapchain_extensions) {
  // No need to create any swapchain if no window is used.
  if (windows.empty()) {
    swapchain_extensions = {};
  }

  instance_ = std::make_unique<Instance>(this, application_name, windows);
  if (debug_message_config.has_value()) {
    debug_callback_ =
        std::make_unique<DebugCallback>(this, debug_message_config.value());
  }

  std::vector<const Surface*> surface_ptrs;
  surfaces_.reserve(windows.size());
  surface_ptrs.reserve(windows.size());
  for (const common::Window* window : windows) {
    surfaces_.push_back(std::make_unique<Surface>(this, *window));
    surface_ptrs.push_back(surfaces_.back().get());
  }

  physical_device_ = std::make_unique<PhysicalDevice>(
      this, surface_ptrs, swapchain_extensions);
  device_ = std::make_unique<Device>(this, swapchain_extensions);
  queues_ = std::make_unique<Queues>(*this);
}

}  // namespace vk::renderer::lighter
