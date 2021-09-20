//
//  context.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/context.h"

#include "lighter/renderer/vk/util.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace lighter::renderer::vk {

Context::Context(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    absl::Span<const common::Window* const> windows) {
  LOG_INFO << "Starting Vulkan initialization";

  const bool enable_validation = debug_message_config.has_value();
  const bool enable_swapchain = !windows.empty();

  // Load instance-independent function pointers.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
  instance_ = std::make_unique<Instance>(this, enable_validation,
                                         application_name, windows);
  // Load instance-dependent function pointers.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(**instance_);
  host_allocator_ = std::make_unique<HostMemoryAllocator>();
  if (enable_validation) {
    debug_messenger_ =
        std::make_unique<DebugMessenger>(this, debug_message_config.value());
  }

  std::vector<Surface*> surface_ptrs;
  surfaces_.reserve(windows.size());
  surface_ptrs.reserve(windows.size());
  for (const common::Window* window : windows) {
    surfaces_.push_back(std::make_unique<Surface>(this, *window));
    surface_ptrs.push_back(surfaces_.back().get());
  }

  physical_device_ = std::make_unique<PhysicalDevice>(this, surface_ptrs);
  device_ = std::make_unique<Device>(this, enable_validation, enable_swapchain);
  // Load device-specific function pointers.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(**device_);
  queues_ = std::make_unique<Queues>(*this);

  LOG_INFO << "Finished Vulkan initialization";
}

}  // namespace lighter::renderer::vk
