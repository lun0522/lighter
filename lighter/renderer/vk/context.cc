//
//  context.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/context.h"

#include "third_party/absl/memory/memory.h"

namespace lighter {
namespace renderer {
namespace vk {

Context::Context(
    absl::string_view application_name,
    const absl::optional<debug_message::Config>& debug_message_config,
    absl::Span<const common::Window* const> windows,
    absl::Span<const char* const> swapchain_extensions) {
  // No need to create any swapchain if no window is used.
  if (windows.empty()) {
    swapchain_extensions = {};
  }

  instance_ = absl::make_unique<Instance>(this, application_name, windows);
  if (debug_message_config.has_value()) {
    debug_callback_ =
        absl::make_unique<DebugCallback>(this, debug_message_config.value());
  }

  std::vector<const Surface*> surface_ptrs;
  surfaces_.reserve(windows.size());
  surface_ptrs.reserve(windows.size());
  for (const common::Window* window : windows) {
    surfaces_.push_back(absl::make_unique<Surface>(this, *window));
    surface_ptrs.push_back(surfaces_.back().get());
  }

  physical_device_ = absl::make_unique<PhysicalDevice>(
      this, surface_ptrs, swapchain_extensions);
  device_ = absl::make_unique<Device>(this, swapchain_extensions);
  queues_ = absl::make_unique<Queues>(*this);
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
