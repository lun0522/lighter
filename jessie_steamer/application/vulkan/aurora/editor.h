//
//  editor.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_H

#include <memory>

#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/common/camera.h"
#include "third_party/absl/types/optional.h"
#include "third_party/glm/glm.hpp"
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

  void UpdateData(const common::Window& window, int frame);

  void Render(const VkCommandBuffer& command_buffer,
              uint32_t framebuffer_index, int current_frame);

 private:
  class EarthManager {
   public:
    explicit EarthManager();

    // This class is neither copyable nor movable.
    EarthManager(const EarthManager&) = delete;
    EarthManager& operator=(const EarthManager&) = delete;

    void Update(const Editor& editor,
                const absl::optional<glm::vec2>& click_ndc);

    // Accessors.
    const glm::mat4& model_matrix() const { return model_matrix_; }

   private:
    void Rotate(const glm::vec3& intersection);

    void InertialRotate();

    bool should_rotate_ = false;
    glm::vec3 last_intersection_;
    glm::vec3 rotate_axis_;
    float rotate_angle_ = 0.0f;
    bool should_inertial_rotate_ = false;
    float inertial_rotate_start_time_ = 0.0f;
    glm::mat4 model_matrix_;
    common::Timer timer_;
  };

  absl::optional<glm::vec3> GetIntersectionWithSphere(
      const glm::vec2& click_ndc, float sphere_radius) const;

  const wrapper::vulkan::SharedBasicContext context_;
  bool is_day_ = false;
  bool is_pressing_left_ = false;
  EarthManager earth_;
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

#endif //JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_H
