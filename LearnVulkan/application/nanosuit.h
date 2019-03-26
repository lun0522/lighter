//
//  nanosuit.h
//
//  Created by Pujun Lun on 3/25/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef APPLICATION_VULKAN_NANOSUIT_H
#define APPLICATION_VULKAN_NANOSUIT_H

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

using namespace wrapper::vulkan;

class NanosuitApp {
 public:
  NanosuitApp() : context_{Context::CreateContext()} {
      context_->Init("Nanosuit");
  };
  void MainLoop();

 private:
  bool should_quit_{false};
  bool is_first_time{true};
  size_t current_frame_{0};
  std::shared_ptr<Context> context_;
  Pipeline pipeline_;
  Command command_;
  Model model_;
  UniformBuffer uniform_buffer_;
  TextureImage image_;
  DepthStencilImage depth_stencil_;
  std::vector<descriptor::ResourceInfo> resource_infos_;
  std::vector<std::unique_ptr<Descriptor>> descriptors_;

  void Init();
  void Cleanup();
};

} /* namespace vulkan */
} /* namespace application */

#endif /* APPLICATION_VULKAN_NANOSUIT_H */
