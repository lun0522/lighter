//
//  geometry_pass.h
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_TROOP_GEOMETRY_PASS_H
#define LIGHTER_APPLICATION_VULKAN_TROOP_GEOMETRY_PASS_H

#include <memory>

#include "lighter/common/camera.h"
#include "lighter/renderer/vulkan/extension/model.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "lighter/renderer/vulkan/wrapper/render_pass_util.h"
#include "lighter/renderer/vulkan/wrapper/window_context.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace troop {

// This class is used to handle the render pass for the geometry pass of
// deferred rendering. Note that since the depth stencil image will be reused
// in the lighting pass which does onscreen rendering, we flip the viewport in
// this pass.
class GeometryPass {
 public:
  GeometryPass(const renderer::vulkan::WindowContext& window_context,
               int num_frames_in_flight,
               float model_scale, const glm::ivec2& num_soldiers,
               const glm::vec2& interval_between_soldiers);

  // This class is neither copyable nor movable.
  GeometryPass(const GeometryPass&) = delete;
  GeometryPass& operator=(const GeometryPass&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(const renderer::vulkan::Image& depth_stencil_image,
                         const renderer::vulkan::Image& position_image,
                         const renderer::vulkan::Image& normal_image,
                         const renderer::vulkan::Image& diffuse_specular_image);

  // Updates per-frame data.
  void UpdatePerFrameData(int frame, const common::Camera& camera);

  // Runs the geometry pass.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) const;

 private:
  // Number of soldiers to render.
  const int num_soldiers_;

  // Objects used for rendering.
  std::unique_ptr<renderer::vulkan::StaticPerInstanceBuffer> center_data_;
  std::unique_ptr<renderer::vulkan::UniformBuffer> trans_uniform_;
  std::unique_ptr<renderer::vulkan::Model> nanosuit_model_;
  std::unique_ptr<renderer::vulkan::DeferredShadingRenderPassBuilder>
      render_pass_builder_;
  std::unique_ptr<renderer::vulkan::RenderPass> render_pass_;
};

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_TROOP_GEOMETRY_PASS_H */
