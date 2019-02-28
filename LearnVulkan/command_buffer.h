//
//  command_buffer.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_COMMAND_BUFFER_H
#define LEARNVULKAN_COMMAND_BUFFER_H

#include <vector>

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

class CommandBuffer {
  public:
    CommandBuffer(const Application& app) : app_{app} {}
    VkResult DrawFrame();
    void Init();
    void Cleanup();
    ~CommandBuffer();
    MARK_NOT_COPYABLE_OR_MOVABLE(CommandBuffer);
    
  private:
    static const size_t kMaxFrameInFlight;
    const Application& app_;
    size_t current_frame_{0};
    bool is_first_time_{true};
    std::vector<VkSemaphore> image_available_semas_;
    std::vector<VkSemaphore> render_finished_semas_;
    std::vector<VkFence> in_flight_fences_;
    VkCommandPool command_pool_;
    std::vector<VkCommandBuffer> command_buffers_;
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_COMMAND_BUFFER_H */
