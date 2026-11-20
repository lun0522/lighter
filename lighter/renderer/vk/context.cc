//
//  context.cc
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/context.h"

#include <cstdint>
#include <limits>

#include "lighter/renderer/vk/util.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace lighter::renderer::vk {

Context::Context(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::span<const common::Window* const> windows) {
  LOG_INFO << "Starting Vulkan initialization";

  const bool enable_validation = debug_message_config.has_value();
  const bool enable_swapchain = !windows.empty();

  // Load instance-independent function pointers.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
  host_allocator_ = std::make_unique<HostMemoryAllocator>();
  instance_ = std::make_unique<Instance>(this, enable_validation,
                                         application_name, windows);
  // Load instance-dependent function pointers.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(**instance_);
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
  device_ = std::make_unique<Device>(this, enable_swapchain);
  // Load device-specific function pointers.
  VULKAN_HPP_DEFAULT_DISPATCHER.init(**device_);
  queues_ = std::make_unique<Queues>(*this);

  const VmaVulkanFunctions vulkan_functions{
      .vkGetInstanceProcAddr =
          VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
      .vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr,
  };
  const VmaAllocatorCreateInfo allocator_info{
      .flags = {},
      .physicalDevice = static_cast<VkPhysicalDevice>(**physical_device_),
      .device = static_cast<VkDevice>(**device_),
      .pAllocationCallbacks = host_allocator_->c_type(),
      .pVulkanFunctions = &vulkan_functions,
      .instance = static_cast<VkInstance>(**instance_),
      .vulkanApiVersion = VK_API_VERSION_1_3,
  };
  ASSERT_SUCCESS(static_cast<intl::Result>(
                     vmaCreateAllocator(&allocator_info, &vma_allocator_)),
                 "Failed to create VMA allocator");

  LOG_INFO << "Finished Vulkan initialization";
}

Context::~Context() {
  if (device_) {
    WaitIdle();
  }
  if (vma_allocator_ != VK_NULL_HANDLE) {
    vmaDestroyAllocator(vma_allocator_);
    vma_allocator_ = VK_NULL_HANDLE;
  }
}

void Context::RunOneTimeGraphicsCommand(
    const std::function<void(intl::CommandBuffer)>& record_commands) const {
  const auto pool_info =
      intl::CommandPoolCreateInfo{}
          .setFlags(intl::CommandPoolCreateFlagBits::eTransient)
          .setQueueFamilyIndex(
              physical_device_->queue_family_indices().graphics);
  const intl::raii::CommandPool command_pool =
      device_->handle().createCommandPool(pool_info, *host_allocator());

  const auto alloc_info = intl::CommandBufferAllocateInfo{}
                              .setCommandPool(*command_pool)
                              .setLevel(intl::CommandBufferLevel::ePrimary)
                              .setCommandBufferCount(1);
  std::vector<intl::raii::CommandBuffer> command_buffers =
      device_->handle().allocateCommandBuffers(alloc_info);
  const intl::raii::CommandBuffer& command_buffer = command_buffers.front();

  const auto begin_info = intl::CommandBufferBeginInfo{}.setFlags(
      intl::CommandBufferUsageFlagBits::eOneTimeSubmit);
  command_buffer.begin(begin_info);
  record_commands(*command_buffer);
  command_buffer.end();

  const intl::raii::Fence fence =
      device_->handle().createFence(intl::FenceCreateInfo{}, *host_allocator());
  const auto cmd_info =
      intl::CommandBufferSubmitInfo{}.setCommandBuffer(*command_buffer);
  const auto submit_info = intl::SubmitInfo2{}.setCommandBufferInfos(cmd_info);
  queues_->graphics_queue().submit2(submit_info, *fence);
  ASSERT_SUCCESS(
      device_->handle().waitForFences({*fence}, intl::True,
                                      std::numeric_limits<uint64_t>::max()),
      "Failed to wait for one-time graphics command fence");
}

}  // namespace lighter::renderer::vk
