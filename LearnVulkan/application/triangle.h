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
