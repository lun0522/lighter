//
//  command.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/command.h"

#include <limits>

#include "lighter/common/util.h"

namespace lighter::renderer::vk {

PerFrameCommand::PerFrameCommand(const SharedContext& context,
                                 int num_frames_in_flight)
    : WithSharedContext{context} {
  ASSERT_TRUE(num_frames_in_flight > 0,
              "num_frames_in_flight must be positive");

  const auto pool_info =
      intl::CommandPoolCreateInfo{}
          .setFlags(intl::CommandPoolCreateFlagBits::eResetCommandBuffer)
          .setQueueFamilyIndex(
              context_->physical_device().queue_family_indices().graphics);
  command_pool_ = vk_device().createCommandPool(pool_info, vk_host_allocator());

  const auto alloc_info =
      intl::CommandBufferAllocateInfo{}
          .setCommandPool(command_pool_)
          .setLevel(intl::CommandBufferLevel::ePrimary)
          .setCommandBufferCount(CAST_TO_UINT(num_frames_in_flight));
  const std::vector<intl::CommandBuffer> command_buffers =
      vk_device().allocateCommandBuffers(alloc_info);

  const auto fence_info =
      intl::FenceCreateInfo{}.setFlags(intl::FenceCreateFlagBits::eSignaled);
  frame_syncs_.resize(num_frames_in_flight);
  for (int i = 0; i < num_frames_in_flight; ++i) {
    frame_syncs_[i] = FrameSync{
        .command_buffer = command_buffers[i],
        .image_available_semaphore = vk_device().createSemaphore(
            intl::SemaphoreCreateInfo{}, vk_host_allocator()),
        .in_flight_fence =
            vk_device().createFence(fence_info, vk_host_allocator()),
    };
  }
}

PerFrameCommand::~PerFrameCommand() {
  for (const auto& sync : frame_syncs_) {
    context_->DeviceDestroy(sync.in_flight_fence);
    context_->DeviceDestroy(sync.image_available_semaphore);
  }
  context_->DeviceDestroy(command_pool_);
}

bool PerFrameCommand::Run(
    int frame_index, const Swapchain& swapchain,
    const std::function<void(intl::CommandBuffer, uint32_t image_index)>&
        record_commands) const {
  const FrameSync& frame = frame_syncs_.at(frame_index);
  ASSERT_SUCCESS(
      vk_device().waitForFences({frame.in_flight_fence}, intl::True,
                                std::numeric_limits<uint64_t>::max()),
      "Failed to wait for in-flight frame fence");

  uint32_t image_index = 0;
  const intl::Result acquire_result =
      swapchain.AcquireNextImage(frame.image_available_semaphore, &image_index);
  if (acquire_result == intl::Result::eErrorOutOfDateKHR) {
    return false;
  }
  ASSERT_TRUE(acquire_result == intl::Result::eSuccess ||
                  acquire_result == intl::Result::eSuboptimalKHR,
              std::format("Failed to acquire swapchain image: {}",
                          intl::to_string(acquire_result)));

  vk_device().resetFences({frame.in_flight_fence});

  const intl::CommandBuffer cmd = frame.command_buffer;
  cmd.reset();
  const auto begin_info = intl::CommandBufferBeginInfo{}.setFlags(
      intl::CommandBufferUsageFlagBits::eOneTimeSubmit);
  cmd.begin(begin_info);
  record_commands(cmd, image_index);
  cmd.end();

  const intl::Semaphore render_finished_semaphore =
      swapchain.render_finished_semaphore(image_index);
  const auto wait_info =
      intl::SemaphoreSubmitInfo{}
          .setSemaphore(frame.image_available_semaphore)
          .setStageMask(intl::PipelineStageFlagBits2::eColorAttachmentOutput);
  const auto cmd_info = intl::CommandBufferSubmitInfo{}.setCommandBuffer(cmd);
  const auto signal_info =
      intl::SemaphoreSubmitInfo{}
          .setSemaphore(render_finished_semaphore)
          .setStageMask(intl::PipelineStageFlagBits2::eAllGraphics);
  const auto submit_info = intl::SubmitInfo2{}
                               .setWaitSemaphoreInfos(wait_info)
                               .setCommandBufferInfos(cmd_info)
                               .setSignalSemaphoreInfos(signal_info);
  context_->queues().graphics_queue().submit2(submit_info,
                                              frame.in_flight_fence);

  const intl::Result present_result =
      swapchain.Present(image_index, render_finished_semaphore);
  if (acquire_result == intl::Result::eSuboptimalKHR ||
      present_result == intl::Result::eErrorOutOfDateKHR ||
      present_result == intl::Result::eSuboptimalKHR) {
    return false;
  }
  ASSERT_SUCCESS(present_result, "Failed to present swapchain image");
  return true;
}

}  // namespace lighter::renderer::vk
