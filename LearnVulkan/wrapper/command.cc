//
//  command.cc
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "command.h"

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::vector;

void RecordCommands(const vector<VkCommandBuffer>& command_buffers,
                    const vector<VkFramebuffer>& framebuffers,
                    const VkExtent2D image_extent,
                    const VkRenderPass& render_pass,
                    const Pipeline& pipeline,
                    const VertexBuffer& vertex_buffer,
                    const UniformBuffer& uniform_buffer) {
  for (size_t i = 0; i < command_buffers.size(); ++i) {
    // start command buffer recording
    VkCommandBufferBeginInfo cmd_begin_info{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        /*pNext=*/nullptr,
        /*flags=*/VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        /*pInheritanceInfo=*/nullptr,
        // .pInheritanceInfo sets what to inherit from primary buffers
        // to secondary buffers
    };

    ASSERT_SUCCESS(vkBeginCommandBuffer(command_buffers[i], &cmd_begin_info),
                   "Failed to begin recording command buffer");

    // start render pass
    VkClearValue clear_color{0.0f, 0.0f, 0.0f, 1.0f};
    VkRenderPassBeginInfo rp_begin_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        /*pNext=*/nullptr,
        render_pass,
        framebuffers[i],
        /*renderArea=*/{
            /*offset=*/{0, 0},
            image_extent,
        },
        /*clearValueCount=*/1,
        &clear_color,  // used for _OP_CLEAR
    };

    // record commends. options:
    //   - VK_SUBPASS_CONTENTS_INLINE: use primary commmand buffer
    //   - VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS: use secondary
    vkCmdBeginRenderPass(command_buffers[i], &rp_begin_info,
                         VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
                      *pipeline);
    uniform_buffer.Bind(command_buffers[i], pipeline.layout(), i);
    vertex_buffer.Draw(command_buffers[i]);
    vkCmdEndRenderPass(command_buffers[i]);

    // end recording
    ASSERT_SUCCESS(vkEndCommandBuffer(command_buffers[i]),
                   "Failed to end recording command buffer");
  }
}

VkCommandPool CreateCommandPool(uint32_t queue_family_index,
                                const VkDevice& device,
                                bool is_transient,
                                const VkAllocationCallbacks* allocator) {
  // create pool to hold command buffers
  VkCommandPoolCreateInfo pool_info{
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      queue_family_index,
  };
  if (is_transient)
    pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

  VkCommandPool pool{};
  ASSERT_SUCCESS(vkCreateCommandPool(device, &pool_info, allocator, &pool),
                 "Failed to create command pool");
  return pool;
}

VkCommandBuffer CreateCommandBuffer(const VkDevice& device,
                                    const VkCommandPool& command_pool) {
  // allocate command buffer
  VkCommandBufferAllocateInfo buffer_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      command_pool,
      // secondary level command buffer can be called from primary level
      /*level=*/VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      /*commandBufferCount=*/1,
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
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      command_pool,
      // secondary level command buffers can be called from primary level
      /*level=*/VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      /*commandBufferCount=*/static_cast<uint32_t>(count),
  };

  vector<VkCommandBuffer> buffers(count);
  ASSERT_SUCCESS(vkAllocateCommandBuffers(device, &buffer_info, buffers.data()),
                 "Failed to allocate command buffers");
  return buffers;
}

} /* namespace */

void Command::OneTimeCommand(
    const VkDevice& device,
    const Queues::Queue& queue,
    const VkAllocationCallbacks* allocator,
    const RecordCommand& on_record) {
  // construct command pool and buffer
  VkCommandPool command_pool = CreateCommandPool(
      queue.family_index, device, true, allocator);
  VkCommandBuffer command_buffer = CreateCommandBuffer(device, command_pool);

  // record command
  VkCommandBufferBeginInfo begin_info{
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      /*pNext=*/nullptr,
      /*flags=*/VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      /*pInheritanceInfo=*/nullptr,
  };
  vkBeginCommandBuffer(command_buffer, &begin_info);
  on_record(command_buffer);
  vkEndCommandBuffer(command_buffer);

  // submit command buffers, wait until finish and cleanup
  VkSubmitInfo submit_info{
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/0,
      /*pWaitSemaphores=*/nullptr,
      /*pWaitDstStageMask=*/nullptr,
      /*commandBufferCount=*/1,
      &command_buffer,
      /*signalSemaphoreCount=*/0,
      /*pSignalSemaphores=*/nullptr,
  };
  vkQueueSubmit(queue.queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue.queue);
  vkDestroyCommandPool(device, command_pool, allocator);
}

VkResult Command::DrawFrame(const UniformBuffer& uniform_buffer,
                            const std::function<void (size_t)>& update_func) {
  // fence was initialized to signaled state
  // so that waiting for it at the beginning is fine
  vkWaitForFences(*context_->device(), 1, &in_flight_fences_[current_frame_],
                  VK_TRUE, std::numeric_limits<uint64_t>::max());

  // update uniform data
  update_func(current_frame_);
  uniform_buffer.Update(current_frame_);

  // acquire swap chain image
  uint32_t image_index;
  VkResult acquire_result = vkAcquireNextImageKHR(
      *context_->device(), *context_->swapchain(),
      std::numeric_limits<uint64_t>::max(),
      image_available_semas_[current_frame_], VK_NULL_HANDLE, &image_index);
  switch (acquire_result) {
    case VK_ERROR_OUT_OF_DATE_KHR: // swap chain can no longer present image
      return acquire_result;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR: // may be considered as good state as well
      break;
    default:
      throw std::runtime_error{"Failed to acquire swap chain image"};
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
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/1,
      wait_semas,
      // we specify one stage for each semaphore, so no need to pass count
      wait_stages,
      /*commandBufferCount=*/1,
      &command_buffers_[image_index],
      /*signalSemaphoreCount=*/1,
      signal_semas,
  };

  // reset to fences unsignaled state
  vkResetFences(*context_->device(), 1, &in_flight_fences_[current_frame_]);
  ASSERT_SUCCESS(
      vkQueueSubmit(context_->queues().graphics.queue, 1, &submit_info,
                    in_flight_fences_[current_frame_]),
      "Failed to submit draw command buffer");

  // present image to screen
  VkPresentInfoKHR present_info{
      VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      /*pNext=*/nullptr,
      /*waitSemaphoreCount=*/1,
      signal_semas,
      /*swapchainCount=*/1,
      &*context_->swapchain(),
      &image_index,  // image for each swap chain
      /*pResults=*/nullptr,
      // may use .pResults to check wether each swap chain rendered successfully
  };

  VkResult present_result = vkQueuePresentKHR(
      context_->queues().present.queue, &present_info);
  switch (present_result) {
    case VK_ERROR_OUT_OF_DATE_KHR: // swap chain can no longer present image
      return present_result;
      break;
    case VK_SUCCESS:
    case VK_SUBOPTIMAL_KHR: // may be considered as good state as well
      break;
    default:
      throw std::runtime_error{"Failed to present swap chain image"};
  }

  current_frame_ = (current_frame_ + 1) % kMaxFrameInFlight;
  return VK_SUCCESS;
}

void Command::Init(std::shared_ptr<Context> context,
                   const Pipeline& pipeline,
                   const VertexBuffer& vertex_buffer,
                   const UniformBuffer& uniform_buffer) {
  context_ = context;

  if (is_first_time_) {
    command_pool_ = CreateCommandPool(
        context_->queues().graphics.family_index, *context_->device(),
        false, context_->allocator());
    image_available_semas_.Init(context_, kMaxFrameInFlight);
    render_finished_semas_.Init(context_, kMaxFrameInFlight);
    in_flight_fences_.Init(context_, kMaxFrameInFlight, true);
    is_first_time_ = false;
  }
  command_buffers_ = CreateCommandBuffers(
      context_->render_pass().framebuffers().size(), *context_->device(),
      command_pool_);
  RecordCommands(command_buffers_, context_->render_pass().framebuffers(),
                 context_->swapchain().extent(), *context_->render_pass(),
                 pipeline, vertex_buffer, uniform_buffer);
}

void Command::Cleanup() {
  vkFreeCommandBuffers(*context_->device(), command_pool_,
                       CONTAINER_SIZE(command_buffers_),
                       command_buffers_.data());
}

Command::~Command() {
  vkDestroyCommandPool(*context_->device(), command_pool_,
                       context_->allocator());
  // command buffers are implicitly cleaned up with command pool
}

} /* namespace vulkan */
} /* namespace wrapper */
