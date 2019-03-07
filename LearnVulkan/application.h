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

#include "command.h"
#include "pipeline_.h"
#include "render_pass.h"
#include "util.h"
#include "validation.h"
#include "wrapper/basic_object.h" // TODO: remove wrapper/
#include "wrapper/buffer.h" // TODO: remove wrapper/
#include "wrapper/swapchain.h" // TODO: remove wrapper/

class GLFWwindow;

namespace vulkan {

using namespace wrapper; // TODO: remove

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
  const Swapchain& swapchain()            const { return swapchain_; }
  const RenderPass& render_pass()         const { return render_pass_; }
  const Pipeline& pipeline()              const { return pipeline_; }
  const Command& command()                const { return command_; }
  const VertexBuffer& vertex_buffer()     const { return vertex_buffer_; }
  const UniformBuffer& uniform_buffer()   const { return uniform_buffer_; }
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
  Swapchain swapchain_;
  RenderPass render_pass_;
  Pipeline pipeline_;
  Command command_;
  VertexBuffer vertex_buffer_;
  UniformBuffer uniform_buffer_;
#ifdef DEBUG
  DebugCallback callback_;
#endif /* DEBUG */

  void InitWindow(uint32_t width, uint32_t height);
  void InitVulkan();
};

} /* namespace vulkan */

#endif /* LEARNVULKAN_APPLICATION_H */
