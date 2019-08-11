//
//  command.h
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H

#include <functional>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/synchronization.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkCommandPool allocates command buffer memory.
 *
 *  Initialization:
 *    Queue family index
 *
 *------------------------------------------------------------------------------
 *
 *  VkCommandBuffer records all operations we want to perform and submit to a
 *    device queue for execution. Primary level command buffers can call
 *    secondary level ones and submit to queues, while secondary levels ones
 *    are not directly submitted.
 *
 *  Initialization:
 *    VkCommandPool
 *    Level (either primary or secondary)
 */
class Command {
 public:
  virtual ~Command() {
    vkDestroyCommandPool(*context_->device(), command_pool_,
                         context_->allocator());
  }

 protected:
  explicit Command(SharedBasicContext context) : context_{std::move(context)} {}

  SharedBasicContext context_;
  VkCommandPool command_pool_;
};

class OneTimeCommand : public Command {
 public:
  using OnRecord = std::function<void(const VkCommandBuffer& command_buffer)>;

  OneTimeCommand(SharedBasicContext context, const Queues::Queue* queue);

  // This class is neither copyable nor movable.
  OneTimeCommand(const OneTimeCommand&) = delete;
  OneTimeCommand& operator=(const OneTimeCommand&) = delete;

  void Run(const OnRecord& on_record);

 private:
  const Queues::Queue* queue_;
  VkCommandBuffer command_buffer_;
};

class PerFrameCommand : public Command {
 public:
  using OnRecord = std::function<void(const VkCommandBuffer& command_buffer,
                                      uint32_t framebuffer_index)>;
  using UpdateData = std::function<void (int current_frame)>;

  PerFrameCommand(SharedBasicContext context, int num_frame_in_flight);

  // This class is neither copyable nor movable.
  PerFrameCommand(const Command&) = delete;
  PerFrameCommand& operator=(const Command&) = delete;

  VkResult Run(int current_frame,
               const VkSwapchainKHR& swapchain,
               const UpdateData& update_data,
               const OnRecord& on_record);

  void Recreate();

 private:
  Semaphores image_available_semas_;
  Semaphores render_finished_semas_;
  Fences in_flight_fences_;
  std::function<void(bool is_first_time)> create_command_buffers_;
  std::vector<VkCommandBuffer> command_buffers_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H */
