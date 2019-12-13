//
//  path.h
//
//  Created by Pujun Lun on 12/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H

#include <array>
#include <memory>
#include <vector>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/types/optional.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class AuroraPath {
 public:
  enum State { kSelected = 0, kUnselected, kNumStates };

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  AuroraPath(const wrapper::vulkan::SharedBasicContext& context,
             float viewport_aspect_ratio, int num_frames_in_flight,
             const std::vector<std::array<glm::vec3, kNumStates>>& path_colors,
             const std::array<float, kNumStates>& path_alphas);

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
  void Draw(const VkCommandBuffer& command_buffer, int frame,
            const absl::optional<int>& selected_path_index);

 private:
  // Vertex buffers for a single aurora path.
  struct PathVertexBuffers {
    std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
        control_points_buffer;
    std::unique_ptr<wrapper::vulkan::DynamicPerVertexBuffer>
        spline_points_buffer;
  };

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // aurora paths does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  std::vector<std::array<glm::vec4, kNumStates>> path_color_alphas_;

  // Number of aurora paths.
  const int num_paths_;

  // Records the number of control points for each aurora path.
  std::vector<int> num_control_points_;

  std::vector<glm::vec4> color_alphas_to_render_;

  // Objects used for rendering.
  std::unique_ptr<wrapper::vulkan::StaticPerVertexBuffer> sphere_vertex_buffer_;
  std::vector<PathVertexBuffers> paths_vertex_buffers_;
  std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
      color_alpha_vertex_buffer_;
  std::unique_ptr<wrapper::vulkan::PushConstant> control_render_constant_;
  std::unique_ptr<wrapper::vulkan::PushConstant> spline_trans_constant_;
  wrapper::vulkan::PipelineBuilder control_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> control_pipeline_;
  wrapper::vulkan::PipelineBuilder spline_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> spline_pipeline_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H */
