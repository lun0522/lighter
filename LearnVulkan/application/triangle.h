//
//  triangle.h
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_APPLICATION_TRIANGLE_H
#define VULKAN_APPLICATION_TRIANGLE_H

#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "buffer.h"
#include "command.h"
#include "context.h"
#include "pipeline.h"

namespace vulkan {
namespace application {

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/
//    chap14.html#interfaces-resources-layout
struct UniformBufferObject {
  alignas(16) glm::mat4 model;
  alignas(16) glm::mat4 view;
  alignas(16) glm::mat4 proj;
};

struct VertexAttrib {
  glm::vec2 pos;
  glm::vec3 color;
  static std::vector<VkVertexInputBindingDescription> binding_descriptions();
  static std::vector<VkVertexInputAttributeDescription> attrib_descriptions();
  static const void* ubo();
  static size_t ubo_size() { return sizeof(glm::mat4) * 3; }
  static void UpdateUbo(size_t current_frame, float screen_aspect);
};

class TriangleApplication {
 public:
  TriangleApplication() : context_{wrapper::Context::CreateContext()} {
      context_->Init("Triangle");
  };
  void MainLoop();

 private:
  bool is_first_time{true};
  std::shared_ptr<wrapper::Context> context_;
  wrapper::Pipeline pipeline_;
  wrapper::Command command_;
  wrapper::VertexBuffer vertex_buffer_;
  wrapper::UniformBuffer uniform_buffer_;

  void Init();
  void Cleanup();
};

} /* namespace application */
} /* namespace vulkan */

#endif /* VULKAN_APPLICATION_TRIANGLE_H */
