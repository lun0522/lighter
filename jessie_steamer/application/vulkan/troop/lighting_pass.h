//
//  lighting_pass.h
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H

#include <vector>
#include <memory>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/timer.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/render_pass_util.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace troop {

// This class is used to handle the render pass for the lighting pass of
// deferred rendering.
class LightingPass {
 public:
  LightingPass(const wrapper::vulkan::WindowContext* window_context,
               int num_frames_in_flight);

  // This class is neither copyable nor movable.
  LightingPass(const LightingPass&) = delete;
  LightingPass& operator=(const LightingPass&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(
      const wrapper::vulkan::Image& depth_stencil_image,
      const wrapper::vulkan::OffscreenImage& position_image,
      const wrapper::vulkan::OffscreenImage& normal_image,
      const wrapper::vulkan::OffscreenImage& diffuse_specular_image);

  // Updates per-frame data.
  void UpdatePerFrameData(int frame, const common::Camera& camera);

  // Runs the lighting pass.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) const;

 private:
  // TODO
  std::vector<glm::vec3> original_light_centers_;

  // Objects used for rendering.
  const wrapper::vulkan::WindowContext& window_context_;
  common::BasicTimer timer_;
  std::unique_ptr<wrapper::vulkan::UniformBuffer> lights_uniform_;
  std::unique_ptr<wrapper::vulkan::UniformBuffer> render_info_uniform_;
  std::vector<std::unique_ptr<wrapper::vulkan::StaticDescriptor>> descriptors_;
  std::unique_ptr<wrapper::vulkan::PerVertexBuffer> squad_vertex_buffer_;
  std::unique_ptr<wrapper::vulkan::GraphicsPipelineBuilder>
      lights_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> lights_pipeline_;
  std::unique_ptr<wrapper::vulkan::GraphicsPipelineBuilder>
      soldiers_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> soldiers_pipeline_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
};

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_LIGHTING_PASS_H */
