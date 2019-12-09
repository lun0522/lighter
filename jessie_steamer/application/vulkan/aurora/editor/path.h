//
//  celestial.h
//
//  Created by Pujun Lun on 12/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H

#include <memory>
#include <vector>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class AuroraPath {
 public:
  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  AuroraPath(const wrapper::vulkan::SharedBasicContext& context,
             int num_paths, float viewport_aspect_ratio,
             int num_frames_in_flight);

  // This class is neither copyable nor movable.
  AuroraPath(const AuroraPath&) = delete;
  AuroraPath& operator=(const AuroraPath&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  void UpdateFramebuffer(
      const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
      const wrapper::vulkan::RenderPass& render_pass, uint32_t subpass_index);

  // Updates the vertex data of aurora path at 'path_index'.
  void UpdatePath(int path_index, const std::vector<glm::vec3>& control_points,
                  const std::vector<glm::vec3>& spline_points);

  // Updates the transformation matrix.
  void UpdateTransMatrix(int frame, const common::Camera& camera,
                         const glm::mat4& model);

  // Renders the aurora paths.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, int frame) const;

 private:
  // Number of aurora paths.
  const int num_paths_;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // aurora paths does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Objects used for rendering.
  std::vector<std::unique_ptr<wrapper::vulkan::DynamicPerVertexBuffer>>
      path_vertex_buffers_;
  std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
      color_alpha_vertex_buffer_;
  std::unique_ptr<wrapper::vulkan::PushConstant> trans_constant_;
  wrapper::vulkan::PipelineBuilder pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H */
