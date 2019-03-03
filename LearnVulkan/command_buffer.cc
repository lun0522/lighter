//
//  command_buffer.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "command_buffer.h"

#include "application.h"
#include "synchronize.h"
#include "vertex_buffer.h"

using namespace std;

namespace vulkan {

namespace {

size_t kMaxFrameInFlight{2};

void RecordCommands(const vector<VkCommandBuffer>& command_buffers,
                    const vector<VkFramebuffer>& framebuffers,
                    const VkExtent2D extent,
                    const VkRenderPass& render_pass,
                    const VkPipeline& pipeline,
                    const VertexBuffer& buffer) {
  for (size_t i = 0; i < command_buffers.size(); ++i) {
    // start command buffer recording
    VkCommandBufferBeginInfo cmd_begin_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    };
    // .pInheritanceInfo sets what to inherit from primary buffers
    // to secondary buffers

    ASSERT_SUCCESS(vkBeginCommandBuffer(command_buffers[i], &cmd_begin_info),
                   "Failed to begin recording command buffer");

    // start render pass
    VkClearValue clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    VkRenderPassBeginInfo rp_begin_info{
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = render_pass,
      .framebuffer = framebuffers[i],
      .renderArea.offset = {0, 0},
      .renderArea.extent = extent,
      .clearValueCount = 1,
      .pClearValues = &clear_color, // used for _OP_CLEAR
    };

    // record commends. options:
    //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
    //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
    vkCmdBeginRenderPass(command_buffers[i], &rp_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);
    buffer.Draw(command_buffers[i]);
    vkCmdEndRenderPass(command_buffers[i]);

    // end recording
    ASSERT_SUCCESS(vkEndCommandBuffer(command_buffers[i]),
                   "Failed to end recording command buffer");
  }
}

} /* namespace */

VkCommandPool CreateCommandPool(uint32_t queue_family_index,
                                const VkDevice& device,
                                bool is_transient) {
  // create pool to hold command buffers
  VkCommandPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
    .queueFamilyIndex = queue_family_index,
  };
  if (is_transient)
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

  VkCommandPool pool{};
  ASSERT_SUCCESS(vkCreateCommandPool(device, &pool_info, nullptr, &pool),
                 "Failed to create command pool");
  return pool;
}

VkCommandBuffer CreateCommandBuffer(const VkDevice& device,
                                    const VkCommandPool& pool) {
  // allocate command buffer
  VkCommandBufferAllocateInfo buffer_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = pool,
    // secondary level command buffer can be called from primary level
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
  };

  VkCommandBuffer buffer{};
  ASSERT_SUCCESS(vkAllocateCommandBuffers(device, &buffer_info, &buffer),
                 "Failed to allocate command buffer");
  return buffer;
}

vector<VkCommandBuffer> CreateCommandBuffers(
    size_t count,
    const VkDevice& device,
    const VkCommandPool& command_pool) {
  // allocate command buffers
  VkCommandBufferAllocateInfo buffer_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = command_pool,
    // secondary level command buffers can be called from primary level
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = static_cast<uint32_t>(count),
  };

  vector<VkCommandBuffer> buffers(count);
  ASSERT_SUCCESS(vkAllocateCommandBuffers(device, &buffer_info, buffers.data()),
                 "Failed to allocate command buffers");
  return buffers;
}

VkResult CommandBuffer::DrawFrame() {
  const VkDevice& device = *app_.device();
  const VkSwapchainKHR& swapchain = *app_.swapchain();
  const Queues& queues = app_.queues();

  // fence was initialized to signaled state
  // so that waiting for it at the beginning is fine
  vkWaitForFences(device, 1, &in_flight_fences_[current_frame_], VK_TRUE,
                  numeric_limits<uint64_t>::max());

  // acquire swap chain image
  uint32_t image_index;
  VkResult acquire_result = vkAcquireNextImageKHR(
      device, swapchain, numeric_limits<uint64_t>::max(),
      image_available_semas_[current_frame_], VK_NULL_HANDLE, &image_index);
  switch (acquire_result) {
    case VK_ERROR_OUT_OF_DATE_KHR: // swap chain can no longer present image
      return acquire_result;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR: // may be considered as good state as well
      break;
    default:
      throw runtime_error{"Failed to acquire swap chain image"};
  }

  // wait for image available
  VkSemaphore wait_semas[]{
    image_available_semas_[current_frame_],
  };
  // we have to wait only if we want to write to color attachment
  // so we actually can start running pipeline long before that image is ready
  VkPipelineStageFlags wait_stages[]{
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };
  // these semas will be signaled once command buffer finishes
  VkSemaphore signal_semas[]{
    render_finished_semas_[current_frame_],
  };
  
  VkSubmitInfo submit_info{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = wait_semas,
    // we specify one stage for each semaphore, so we don't need to pass count
    .pWaitDstStageMask = wait_stages,
    .commandBufferCount = 1,
    .pCommandBuffers = &command_buffers_[image_index],
    .signalSemaphoreCount = 1,
    .pSignalSemaphores = signal_semas,
  };

  // reset to fences unsignaled state
  vkResetFences(device, 1, &in_flight_fences_[current_frame_]);
  ASSERT_SUCCESS(vkQueueSubmit(queues.graphics.queue, 1, &submit_info,
                               in_flight_fences_[current_frame_]),
                 "Failed to submit draw command buffer");

  // present image to screen
  VkSwapchainKHR swapchains[]{
    swapchain,
  };
  
  VkPresentInfoKHR present_info{
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = signal_semas,
    .swapchainCount = 1,
    .pSwapchains = swapchains,
    .pImageIndices = &image_index, // image for each swap chain
    // may use .pResults to check wether each swap chain rendered successfully
  };

  VkResult present_result = vkQueuePresentKHR(
      queues.present.queue, &present_info);
  switch (present_result) {
    case VK_ERROR_OUT_OF_DATE_KHR: // swap chain can no longer present image
      return present_result;
      break;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR: // may be considered as good state as well
      break;
    default:
      throw runtime_error{"Failed to present swap chain image"};
  }

  current_frame_ = (current_frame_ + 1) % kMaxFrameInFlight;
  return VK_SUCCESS;
}

void CommandBuffer::Init() {
  const VkDevice& device = *app_.device();
  const VkRenderPass& render_pass = *app_.render_pass();
  const VkPipeline& pipeline = *app_.pipeline();
  const VertexBuffer& buffer = app_.vertex_buffer();
  const Queues::Queue& graphics_queue = app_.queues().graphics;
  const vector<VkFramebuffer>& framebuffers = app_.render_pass().framebuffers();
  const VkExtent2D extent = app_.swapchain().extent();

  if (is_first_time_) {
    command_pool_ = CreateCommandPool(graphics_queue.family_index, device);
    image_available_semas_ = CreateSemaphores(kMaxFrameInFlight, device);
    render_finished_semas_ = CreateSemaphores(kMaxFrameInFlight, device);
    in_flight_fences_ = CreateFences(kMaxFrameInFlight, device, true);
    is_first_time_ = false;
  }
  command_buffers_ = CreateCommandBuffers(
      framebuffers.size(), device, command_pool_);
  RecordCommands(command_buffers_, framebuffers, extent, render_pass, pipeline,
                 buffer);
}

void CommandBuffer::Cleanup() {
  const VkDevice& device = *app_.device();
  vkFreeCommandBuffers(device, command_pool_, CONTAINER_SIZE(command_buffers_),
                       command_buffers_.data());
}

CommandBuffer::~CommandBuffer() {
  const VkDevice& device = *app_.device();
  vkDestroyCommandPool(device, command_pool_, nullptr);
  // command buffers are implicitly cleaned up with command pool
  for (size_t i = 0; i < kMaxFrameInFlight; ++i) {
    vkDestroySemaphore(device, image_available_semas_[i], nullptr);
    vkDestroySemaphore(device, render_finished_semas_[i], nullptr);
    vkDestroyFence(device, in_flight_fences_[i], nullptr);
  }
}

} /* namespace vulkan */
