//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_H

#include <memory>

#include "jessie_steamer/application/vulkan/aurora/button.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/rotation.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class Editor {
 public:
  Editor(const wrapper::vulkan::WindowContext& window_context,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Editor(const Editor&) = delete;
  Editor& operator=(const Editor&) = delete;

  // Registers callbacks.
  void OnEnter(common::Window* mutable_window);

  // Unregisters callbacks.
  void OnExit(common::Window* mutable_window);

  void Recreate(const wrapper::vulkan::WindowContext& window_context);

  void UpdateData(const wrapper::vulkan::WindowContext& window_context,
                  int frame);

  void Render(const VkCommandBuffer& command_buffer,
              uint32_t framebuffer_index, int current_frame);

 private:
  const wrapper::vulkan::SharedBasicContext context_;
  const float original_aspect_ratio_;
  bool is_day_ = false;
  bool is_pressing_left_ = false;
  common::Sphere earth_;
  std::unique_ptr<common::UserControlledCamera> camera_;
  std::unique_ptr<wrapper::vulkan::UniformBuffer> uniform_buffer_;
  std::unique_ptr<wrapper::vulkan::PushConstant> earth_constant_;
  std::unique_ptr<wrapper::vulkan::PushConstant> skybox_constant_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
  std::unique_ptr<wrapper::vulkan::Image> depth_stencil_image_;
  std::unique_ptr<wrapper::vulkan::Model> earth_model_;
  std::unique_ptr<wrapper::vulkan::Model> skybox_model_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_H */
