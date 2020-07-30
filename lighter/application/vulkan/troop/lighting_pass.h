//
//  lighting_pass.h
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H
#define LIGHTER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H

#include <vector>
#include <memory>

#include "lighter/common/camera.h"
#include "lighter/common/timer.h"
#include "lighter/renderer/vulkan/extension/attachment_info.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "lighter/renderer/vulkan/wrapper/window_context.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace troop {

// This class is used to handle the render pass for the lighting pass of
// deferred rendering.
class LightingPass {
 public:
  // Centers of lights will be randomly generated within bounds, and moves by
  // the specified increments. Increments are measured in the change of world
  // coordinates per second.
  struct LightCenterConfig {
    glm::vec2 bound_x;
    glm::vec2 bound_y;
    glm::vec2 bound_z;
    glm::vec3 increments;
  };

  LightingPass(const renderer::vulkan::WindowContext* window_context,
               int num_frames_in_flight, const LightCenterConfig& config);

  // This class is neither copyable nor movable.
  LightingPass(const LightingPass&) = delete;
  LightingPass& operator=(const LightingPass&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(
      const renderer::vulkan::Image& depth_stencil_image,
      const renderer::vulkan::OffscreenImage& position_image,
      const renderer::vulkan::OffscreenImage& normal_image,
      const renderer::vulkan::OffscreenImage& diffuse_specular_image);

  // Updates per-frame data.
  void UpdatePerFrameData(int frame, const common::Camera& camera,
                          float light_model_scale);

  // Runs the lighting pass.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) const;

 private:
  // Populates 'render_pass_builder_'.
  void CreateRenderPassBuilder(
      const renderer::vulkan::Image& depth_stencil_image);

  // Configures how do we generate 'original_light_centers_' and how do centers
  // change over time.
  const LightCenterConfig light_center_config_;

  // Original centers of lights.
  const std::vector<glm::vec3> original_light_centers_;

  // Used to get the elapsed time.
  const common::BasicTimer timer_;

  // Objects used for rendering.
  const renderer::vulkan::WindowContext& window_context_;
  renderer::vulkan::AttachmentInfo swapchain_image_info_{"Swapchain"};
  renderer::vulkan::AttachmentInfo depth_stencil_image_info_{"Depth stencil"};
  std::unique_ptr<renderer::vulkan::UniformBuffer> lights_colors_uniform_;
  std::unique_ptr<renderer::vulkan::UniformBuffer> render_info_uniform_;
  std::unique_ptr<renderer::vulkan::PushConstant> lights_trans_constant_;
  std::vector<std::unique_ptr<renderer::vulkan::StaticDescriptor>>
      lights_descriptors_;
  std::vector<std::unique_ptr<renderer::vulkan::StaticDescriptor>>
      soldiers_descriptors_;
  std::unique_ptr<renderer::vulkan::PerVertexBuffer> cube_vertex_buffer_;
  std::unique_ptr<renderer::vulkan::PerVertexBuffer> squad_vertex_buffer_;
  std::unique_ptr<renderer::vulkan::GraphicsPipelineBuilder>
      lights_pipeline_builder_;
  std::unique_ptr<renderer::vulkan::Pipeline> lights_pipeline_;
  std::unique_ptr<renderer::vulkan::GraphicsPipelineBuilder>
      soldiers_pipeline_builder_;
  std::unique_ptr<renderer::vulkan::Pipeline> soldiers_pipeline_;
  std::unique_ptr<renderer::vulkan::RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<renderer::vulkan::RenderPass> render_pass_;
};

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H */
