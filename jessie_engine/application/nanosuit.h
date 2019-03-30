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
#include "camera.h"
#include "command.h"
#include "context.h"
#include "image.h"
#include "model.h"
#include "pipeline.h"
#include "util.h"

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
  bool should_quit_ = false;
  bool is_first_time = true;
  size_t current_frame_ = 0;
  util::TimePoint last_time_;
  std::shared_ptr<Context> context_;
  std::unique_ptr<camera::Camera> camera_;
  Command command_;
  UniformBuffer uniform_buffer_;
  DepthStencilImage depth_stencil_;
  Pipeline cube_pipeline_, skybox_pipeline_;
  Model cube_model_, skybox_model_;
  TextureImage cube_tex_, skybox_tex_;
  std::vector<descriptor::ResourceInfo> cude_rsrc_infos_, skybox_rsrc_infos_;
  std::vector<std::unique_ptr<Descriptor>> cube_dscs_, skybox_dscs_;

  void Init();
  void Cleanup();
  void UpdateTrans(size_t image_index);
};

} /* namespace vulkan */
} /* namespace application */

#endif /* APPLICATION_VULKAN_NANOSUIT_H */
