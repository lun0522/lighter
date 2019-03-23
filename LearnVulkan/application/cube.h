//
//  cube.h
//
//  Created by Pujun Lun on 3/2/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef APPLICATION_VULKAN_CUBE_H
#define APPLICATION_VULKAN_CUBE_H

#include <memory>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

#include "buffer.h"
#include "command.h"
#include "context.h"
#include "image.h"
#include "model.h"
#include "pipeline.h"

namespace application {
namespace vulkan {

class CubeApplication {
 public:
  CubeApplication() : context_{wrapper::vulkan::Context::CreateContext()} {
      context_->Init("Cube");
  };
  void MainLoop();

 private:
  bool is_first_time{true};
  size_t current_frame_{0};
  std::shared_ptr<wrapper::vulkan::Context> context_;
  wrapper::vulkan::Pipeline pipeline_;
  wrapper::vulkan::Command command_;
  wrapper::vulkan::Model model_;
  wrapper::vulkan::UniformBuffer uniform_buffer_;
  wrapper::vulkan::TextureImage image_;
  wrapper::vulkan::DepthStencilImage depth_stencil_;
  std::vector<wrapper::vulkan::descriptor::ResourceInfo> resource_infos_;
  std::vector<std::unique_ptr<wrapper::vulkan::Descriptor>> descriptors_;

  void Init();
  void Cleanup();
};

} /* namespace vulkan */
} /* namespace application */

#endif /* APPLICATION_VULKAN_CUBE_H */
