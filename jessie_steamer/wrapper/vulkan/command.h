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

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/synchronization.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// VkCommandBuffer records operations that we want to perform, and submits to a
// device queue for execution. It is allocated from VkCommandPool.
// Both primary level and secondary level command buffers can record commands,
// but only the primary can be submitted to the queue. The secondary can be
// built in different threads and executed in different primary command buffers.
// This is the base class of all command classes. The user should use it through
// derived classes. Since all commands need VkCommandPool, which allocates
// command buffers, it will be held and destroyed by this base class, and
// initialized by derived classes.
class Command {
 public:
  virtual ~Command() {
    vkDestroyCommandPool(*context_->device(), command_pool_,
                         *context_->allocator());
  }

 protected:
  explicit Command(SharedBasicContext context)
      : context_{std::move(FATAL_IF_NULL(context))} {}

  // Modifiers.
  void SetCommandPool(const VkCommandPool& command_pool) {
    command_pool_ = command_pool;
  }

  // Pointer to context.
  const SharedBasicContext context_;

 private:
  // Opaque command pool object.
  VkCommandPool command_pool_;
};

// This class creates a command that is meant to be executed for only once.
class OneTimeCommand : public Command {
 public:
  // Specifies which operations should be performed.
  using OnRecord = std::function<void(const VkCommandBuffer& command_buffer)>;

  // The recorded operations will be submitted to 'queue'.
  OneTimeCommand(SharedBasicContext context, const Queues::Queue* queue);

  // This class is neither copyable nor movable.
  OneTimeCommand(const OneTimeCommand&) = delete;
  OneTimeCommand& operator=(const OneTimeCommand&) = delete;

  // Executes the command once and waits for completion.
  void Run(const OnRecord& on_record) const;

 private:
  // Used to execute the command.
  const Queues::Queue* queue_;

  // Opaque command buffer object.
  VkCommandBuffer command_buffer_;
};

// This classes creates a command that will be executed in every frame.
// It assumes that the user is doing on-screen rendering, and handles the
// synchronization internally.
class PerFrameCommand : public Command {
 public:
  // Specifies which operations should be performed. Since the swapchain holds
  // several images, 'framebuffer_index' will be the index of the swapchain
  // image used in this recording.
  using OnRecord = std::function<void(const VkCommandBuffer& command_buffer,
                                      uint32_t framebuffer_index)>;

  // The user may want to do multiple buffering. 'current_frame' refers to which
  // "buffer" are we rendering to.
  using UpdateData = std::function<void (int current_frame)>;

  // Our rendering is 'num_frames_in_flight'-buffered.
  PerFrameCommand(const SharedBasicContext& context, int num_frames_in_flight);

  // This class is neither copyable nor movable.
  PerFrameCommand(const Command&) = delete;
  PerFrameCommand& operator=(const Command&) = delete;

  // Records operations for a new frame and submits to the graphics queue,
  // without waiting for completion. The return value can be:
  //   - absl::nullopt, if the swapchain can be kept using, or
  //   - otherwise, if the swapchain need to be rebuilt.
  // If any unexpected error occurs, a runtime exception will be thrown.
  absl::optional<VkResult> Run(int current_frame,
                               const VkSwapchainKHR& swapchain,
                               const UpdateData& update_data,
                               const OnRecord& on_record);

 private:
  // Opaque command buffer objects.
  std::vector<VkCommandBuffer> command_buffers_;

  // These are used for synchronization. See comments in Run() for details.
  Semaphores image_available_semas_;
  Semaphores render_finished_semas_;
  Fences in_flight_fences_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H */
