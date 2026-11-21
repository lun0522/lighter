//
//  command.h
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_COMMAND_H
#define LIGHTER_RENDERER_VK_COMMAND_H

#include <cstdint>
#include <functional>
#include <vector>

#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/swapchain.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Manages a graphics command pool and per-frame synchronization primitives
// (command buffers, image-available semaphores, and in-flight fences) for
// recording and presenting frames to a Swapchain.
class PerFrameCommand : public WithSharedContext {
 public:
  PerFrameCommand(const SharedContext& context, int num_frames_in_flight);

  // This class is neither copyable nor movable.
  PerFrameCommand(const PerFrameCommand&) = delete;
  PerFrameCommand& operator=(const PerFrameCommand&) = delete;

  ~PerFrameCommand() override;

  // Waits on 'frame_index' fence, acquires the next swapchain image, invokes
  // 'record_commands' to populate the command buffer, submits via submit2, and
  // presents to 'swapchain'. Returns false if the swapchain is out of date or
  // suboptimal and should be recreated.
  bool Run(int frame_index, const Swapchain& swapchain,
           const std::function<void(intl::CommandBuffer, uint32_t image_index)>&
               record_commands) const;

  int num_frames_in_flight() const {
    return static_cast<int>(frame_syncs_.size());
  }

 private:
  struct FrameSync {
    intl::CommandBuffer command_buffer;
    intl::Semaphore image_available_semaphore;
    intl::Fence in_flight_fence;
  };

  intl::CommandPool command_pool_;
  std::vector<FrameSync> frame_syncs_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_COMMAND_H
