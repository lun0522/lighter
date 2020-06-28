//
//  command.h
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_COMMAND_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_COMMAND_H

#include <functional>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/synchronization.h"
#include "third_party/absl/types/optional.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
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
  // This class is neither copyable nor movable.
  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  virtual ~Command() {
    vkDestroyCommandPool(*context_->device(), command_pool_,
                         *context_->allocator());
  }

 protected:
  explicit Command(SharedBasicContext context)
      : context_{std::move(FATAL_IF_NULL(context))} {}

  // Modifiers.
  void set_command_pool(const VkCommandPool& command_pool) {
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
// It assumes that the user is doing onscreen rendering, and handles the
// synchronization internally.
class PerFrameCommand : public Command {
 public:
  // The user may want to do multiple buffering. 'current_frame' refers to which
  // "buffer" are we rendering to.
  using UpdateData = std::function<void(int current_frame)>;

  // Specifies which operations should be performed in the offscreen pass.
  using OffscreenOp =
      std::function<void(const VkCommandBuffer& command_buffer)>;

  // Specifies which operations should be performed in the onscreen pass. Since
  // the swapchain holds several images, 'framebuffer_index' will be the index
  // of the swapchain image used in this recording.
  using OnscreenOp = std::function<void(const VkCommandBuffer& command_buffer,
                                        uint32_t framebuffer_index)>;

  // Our rendering is 'num_frames_in_flight'-buffered.
  PerFrameCommand(const SharedBasicContext& context, int num_frames_in_flight,
                  bool has_offscreen_pass = false);

  // This class is neither copyable nor movable.
  PerFrameCommand(const PerFrameCommand&) = delete;
  PerFrameCommand& operator=(const PerFrameCommand&) = delete;

  // Records operations for a new frame and submits to the graphics queue,
  // without waiting for completion. If 'has_offscreen_pass' passed to the
  // constructor is false, 'offscreen_op' will be ignored.
  // The return value can be:
  //   - absl::nullopt, if the swapchain can be kept using, or
  //   - otherwise, if the swapchain need to be rebuilt.
  // If any unexpected error occurs, a runtime exception will be thrown.
  absl::optional<VkResult> Run(int current_frame,
                               const VkSwapchainKHR& swapchain,
                               const UpdateData& update_data,
                               const OnscreenOp& onscreen_op,
                               const OffscreenOp& offscreen_op = nullptr);

 private:
  // Holds objects used for the offscreen pass.
  struct OffscreenObjects {
    OffscreenObjects(const SharedBasicContext& context,
                     int num_frames_in_flight,
                     std::vector<VkCommandBuffer>&& command_buffers)
        : command_buffers{std::move(command_buffers)},
          semaphores{context, num_frames_in_flight} {}

    // Opaque command buffer objects.
    std::vector<VkCommandBuffer> command_buffers;

    // Used for synchronization. See comments in Run() for details.
    Semaphores semaphores;
  };

  // Opaque command buffer objects.
  std::vector<VkCommandBuffer> command_buffers_;

  // Used for synchronization. See comments in Run() for details.
  Semaphores present_finished_semas_;
  Semaphores render_finished_semas_;
  Fences in_flight_fences_;

  // Used for the offscreen pass.
  absl::optional<OffscreenObjects> offscreen_objects_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_COMMAND_H */
