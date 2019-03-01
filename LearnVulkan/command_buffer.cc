//
//  command_buffer.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "command_buffer.h"

#include "application.h"
#include "vertex_buffer.h"

using namespace std;

namespace vulkan {

namespace {

size_t kMaxFrameInFlight{2};

void CreateCommandPool(VkCommandPool* command_pool,
                       uint32_t queue_family_index,
                       const VkDevice& device) {
  // create a pool to hold command buffers
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = queue_family_index;

  ASSERT_SUCCESS(vkCreateCommandPool(device, &pool_info, nullptr, command_pool),
                 "Failed to create command pool");
}

void CreateCommandBuffers(vector<VkCommandBuffer>* command_buffers,
                          size_t count,
                          const VkDevice& device,
                          const VkCommandPool& command_pool) {
  // allocate command buffers
  command_buffers->resize(count);
  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = command_pool;
  // secondary level command buffers can be called from primary level
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = static_cast<uint32_t>(count);

  ASSERT_SUCCESS(vkAllocateCommandBuffers(
                   device, &alloc_info, command_buffers->data()),
                 "Failed to allocate command buffers");
}

void RecordCommands(const vector<VkCommandBuffer>& command_buffers,
                    const vector<VkFramebuffer>& framebuffers,
                    const VkExtent2D extent,
                    const VkRenderPass& render_pass,
                    const VkPipeline& pipeline,
                    const VertexBuffer& buffer) {
  for (size_t i = 0; i < command_buffers.size(); ++i) {
    // start command buffer recording
    VkCommandBufferBeginInfo cmd_begin_info{};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    // .pInheritanceInfo sets what to inherit from primary buffers
    // to secondary buffers

    ASSERT_SUCCESS(vkBeginCommandBuffer(command_buffers[i], &cmd_begin_info),
                   "Failed to begin recording command buffer");

    // start render pass
    VkRenderPassBeginInfo rp_begin_info{};
    rp_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp_begin_info.renderPass = render_pass;
    rp_begin_info.framebuffer = framebuffers[i];
    rp_begin_info.renderArea.offset = {0, 0};
    rp_begin_info.renderArea.extent = extent;
    VkClearValue clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    rp_begin_info.clearValueCount = 1;
    rp_begin_info.pClearValues = &clear_color; // used for _OP_CLEAR

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

void CreateSemaphore(vector<VkSemaphore>* semas,
                     size_t count,
                     const VkDevice& device) {
  VkSemaphoreCreateInfo sema_info{};
  sema_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  semas->resize(count);
  for (auto& sema : *semas) {
    ASSERT_SUCCESS(vkCreateSemaphore(device, &sema_info, nullptr, &sema),
                   "Failed to create semaphore");
  }
}

void CreateFence(vector<VkFence>* fences,
                 size_t count,
                 const VkDevice& device) {
  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  fences->resize(count);
  for (auto& fence : *fences) {
    ASSERT_SUCCESS(vkCreateFence(device, &fence_info, nullptr, &fence),
                   "Failed to create in flight fence");
  }
}

} /* namespace */

VkResult CommandBuffer::DrawFrame() {
  const VkDevice& device = *app_.device();
  const VkSwapchainKHR& swap_chain = *app_.swap_chain();
  const Queues& queues = app_.queues();

  // fence was initialized to signaled state
  // so that waiting for it at the beginning is fine
  vkWaitForFences(device, 1, &in_flight_fences_[current_frame_], VK_TRUE,
                  numeric_limits<uint64_t>::max());

  // acquire swap chain image
  uint32_t image_index;
  VkResult acquire_result = vkAcquireNextImageKHR(
    device, swap_chain, numeric_limits<uint64_t>::max(),
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
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  VkSemaphore wait_semas[]{image_available_semas_[current_frame_]};
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semas;
  // we have to wait only if we want to write to color attachment
  // so we actually can start running pipeline long before that image is ready
  // we specify one stage for each semaphore, so we don't need to pass count
  VkPipelineStageFlags wait_stages[]{
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.pWaitDstStageMask = wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers_[image_index];
  // these semas will be signaled once command buffer finishes
  VkSemaphore signal_semas[]{render_finished_semas_[current_frame_]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_semas;

  // reset to fences unsignaled state
  vkResetFences(device, 1, &in_flight_fences_[current_frame_]);
  ASSERT_SUCCESS(vkQueueSubmit(queues.graphics.queue, 1, &submit_info,
                               in_flight_fences_[current_frame_]),
                 "Failed to submit draw command buffer");

  // present image to screen
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_semas;
  VkSwapchainKHR swap_chains[]{swap_chain};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swap_chains;
  present_info.pImageIndices = &image_index; // image for each swap chain
  // may use .pResults to check wether each swap chain rendered successfully

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
  const VkExtent2D extent = app_.swap_chain().extent();

  if (is_first_time_) {
    CreateCommandPool(&command_pool_, graphics_queue.family_index, device);
    CreateSemaphore(&image_available_semas_, kMaxFrameInFlight, device);
    CreateSemaphore(&render_finished_semas_, kMaxFrameInFlight, device);
    CreateFence(&in_flight_fences_, kMaxFrameInFlight, device);
    is_first_time_ = false;
  }
  CreateCommandBuffers(&command_buffers_, framebuffers.size(), device,
                       command_pool_);
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
