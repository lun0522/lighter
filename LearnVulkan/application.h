//
//  application.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_APPLICATION_H
#define LEARNVULKAN_APPLICATION_H

#include <string>

#include <vulkan/vulkan.hpp>

#include "basic_object.h"
#include "command_buffer.h"
#include "pipeline_.h"
#include "render_pass.h"
#include "swap_chain.h"
#include "util.h"
#include "validation.h"
#include "vertex_buffer.h"

class GLFWwindow;

namespace vulkan {

class Application {
  public:
    Application(const std::string& vert_file,
                const std::string& frag_file,
                uint32_t width  = 800,
                uint32_t height = 600);
    void MainLoop();
    void Recreate();
    void Cleanup();
    ~Application();
    MARK_NOT_COPYABLE_OR_MOVABLE(Application);
    
    bool& resized()                               { return has_resized_; }
    VkExtent2D current_extent()             const;
    GLFWwindow* window()                    const { return window_; }
    const Instance& instance()              const { return instance_; }
    const Surface& surface()                const { return surface_; }
    const PhysicalDevice& physical_device() const { return physical_device_; }
    const Device& device()                  const { return device_; }
    const SwapChain& swap_chain()           const { return swap_chain_; }
    const RenderPass& render_pass()         const { return render_pass_; }
    const Pipeline& pipeline()              const { return pipeline_; }
    const CommandBuffer& command_buffer()   const { return command_buffer_; }
    const VertexBuffer& vertex_buffer()     const { return vertex_buffer_; }
    const Queues& queues()                  const { return queues_; }
    Queues& queues()                              { return queues_; }
    
  private:
    bool has_resized_{false};
    bool is_first_time_{true};
    GLFWwindow* window_;
    Instance instance_;
    Surface surface_;
    PhysicalDevice physical_device_;
    Device device_;
    Queues queues_;
    SwapChain swap_chain_;
    RenderPass render_pass_;
    Pipeline pipeline_;
    CommandBuffer command_buffer_;
    VertexBuffer vertex_buffer_;
#ifdef DEBUG
    DebugCallback callback_;
#endif /* DEBUG */
    
    void InitWindow(uint32_t width, uint32_t height);
    void InitVulkan();
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_APPLICATION_H */
