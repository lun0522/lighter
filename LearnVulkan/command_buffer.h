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

using std::vector;
class Application;

class CommandBuffer {
    static const size_t kMaxFrameInFlight = 2;
    
    const Application& app_;
    size_t current_frame_ = 0;
    bool first_time_ = true;
    vector<VkSemaphore> image_available_semas_;
    vector<VkSemaphore> render_finished_semas_;
    vector<VkFence> in_flight_fences_;
    VkCommandPool command_pool_;
    vector<VkCommandBuffer> command_buffers_;
    
public:
    CommandBuffer(const Application& app) : app_{app} {}
    VkResult DrawFrame();
    void Init();
    void Cleanup();
    ~CommandBuffer();
    MARK_NOT_COPYABLE_OR_MOVABLE(CommandBuffer);
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_COMMAND_BUFFER_H */
