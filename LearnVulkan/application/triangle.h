//
//  triangle.h
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef APPLICATION_VULKAN_TRIANGLE_H
#define APPLICATION_VULKAN_TRIANGLE_H

#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "buffer.h"
#include "command.h"
#include "context.h"
#include "image.h"
#include "pipeline.h"

namespace application {
namespace vulkan {

class TriangleApplication {
 public:
  TriangleApplication() : context_{wrapper::vulkan::Context::CreateContext()} {
      context_->Init("Triangle");
  };
  void MainLoop();

 private:
  bool is_first_time{true};
  size_t current_frame_{0};
  std::shared_ptr<wrapper::vulkan::Context> context_;
  wrapper::vulkan::Pipeline pipeline_;
  wrapper::vulkan::Command command_;
  wrapper::vulkan::VertexBuffer vertex_buffer_;
  wrapper::vulkan::UniformBuffer uniform_buffer_;
  wrapper::vulkan::Descriptor uniform_desc_;
  wrapper::vulkan::Images images_;

  void Init();
  void Cleanup();
};

} /* namespace vulkan */
} /* namespace application */

#endif /* APPLICATION_VULKAN_TRIANGLE_H */
