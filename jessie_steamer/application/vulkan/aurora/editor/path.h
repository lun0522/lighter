//
//  path.h
//
//  Created by Pujun Lun on 12/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "jessie_steamer/application/vulkan/aurora/editor/button_util.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/common/spline.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used for rendering splines and control points to represent
// aurora paths, and the viewpoint of user on the earth model.
// It should not handle any logic that can be shared with other graphics APIs.
class PathRenderer {
 public:
  PathRenderer(const wrapper::vulkan::SharedBasicContext& context,
               int num_frames_in_flight, int num_paths);

  // This class is neither copyable nor movable.
  PathRenderer(const PathRenderer&) = delete;
  PathRenderer& operator=(const PathRenderer&) = delete;

  // Updates the vertex data of aurora path at 'path_index'.
  void UpdatePath(int path_index,
                  absl::Span<const glm::vec3> control_points,
                  absl::Span<const glm::vec3> spline_points);

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(
      VkSampleCountFlagBits sample_count,
      const wrapper::vulkan::RenderPass& render_pass, uint32_t subpass_index,
      const wrapper::vulkan::PipelineBuilder::ViewportInfo& viewport);

  // Updates per-frame data. This should be called before calling DrawSplines()
  // and DrawControlPoints().
  void UpdatePerFrameData(int frame, float control_point_scale,
                          const glm::mat4& proj_view_model);

  // Renders control points for the aurora path at 'path_index'.
  // This should be called when 'command_buffer' is recording commands.
  void DrawControlPoints(const VkCommandBuffer& command_buffer, int frame,
                         int path_index, const glm::vec4& color_alpha);

  // Renders all splines that represent aurora paths. The length of
  // 'color_alphas' must match with the number of aurora paths.
  // This should be called when 'command_buffer' is recording commands.
  void DrawSplines(const VkCommandBuffer& command_buffer, int frame,
                   absl::Span<const glm::vec4> color_alphas);

  // Renders viewpoint of user on the earth model.
  // This should be called when 'command_buffer' is recording commands.
  void DrawViewpoint(const VkCommandBuffer& command_buffer, int frame,
                     const glm::vec3& center, const glm::vec4& color_alpha);

 private:
  // Vertex buffers for a single aurora path.
  struct PathVertexBuffers {
    std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
        control_points_buffer;
    std::unique_ptr<wrapper::vulkan::DynamicPerVertexBuffer>
        spline_points_buffer;
  };

  // Number of aurora paths.
  const int num_paths_;

  // Records the number of control points for each aurora path.
  std::vector<int> num_control_points_per_path_;

  // Objects used for rendering.
  std::unique_ptr<wrapper::vulkan::StaticPerVertexBuffer> sphere_vertex_buffer_;
  std::vector<PathVertexBuffers> paths_vertex_buffers_;
  std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
      color_alpha_vertex_buffer_;
  std::unique_ptr<wrapper::vulkan::PushConstant> control_render_constant_;
  std::unique_ptr<wrapper::vulkan::PushConstant> spline_trans_constant_;
  std::unique_ptr<wrapper::vulkan::PushConstant> viewpoint_render_constant_;
  wrapper::vulkan::PipelineBuilder control_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> control_pipeline_;
  wrapper::vulkan::PipelineBuilder spline_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> spline_pipeline_;
  wrapper::vulkan::PipelineBuilder viewpoint_pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> viewpoint_pipeline_;
};

// This class is used to render aurora paths and the user viewpoint, and handle
// user inputs.
class AuroraPath {
 public:
  // Returns the initial control points of the aurora path at 'path_index'.
  using GenerateControlPoints =
      std::function<std::vector<glm::vec3>(int path_index)>;

  // Contains information for rendering aurora paths. 'control_point_radius' is
  // measured in the screen coordinate with range (0.0, 1.0]. The length of
  // 'path_colors' determines the number of aurora paths to be rendered.
  struct Info {
    int max_num_control_points;
    float control_point_radius;
    int max_recursion_depth;
    float spline_roughness;
    std::array<glm::vec3, button::kNumStates> viewpoint_colors;
    absl::Span<const std::array<glm::vec3, button::kNumStates>> path_colors;
    std::array<float, button::kNumStates> path_alphas;
    GenerateControlPoints generate_control_points;
  };

  // Describes a user click. Note that paths only respond to left mouse button
  // press or right button release.
  struct ClickInfo {
    absl::optional<int> path_index;
    bool is_left_click;
    glm::vec3 click_object_space;
  };

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  AuroraPath(const wrapper::vulkan::SharedBasicContext& context,
             int num_frames_in_flight, float viewport_aspect_ratio,
             const Info& info);

  // This class is neither copyable nor movable.
  AuroraPath(const AuroraPath&) = delete;
  AuroraPath& operator=(const AuroraPath&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  void UpdateFramebuffer(
      const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
      const wrapper::vulkan::RenderPass& render_pass, uint32_t subpass_index);

  // Updates per-frame data. Note that all control points and spline points are
  // on a unit sphere, hence the 'model' matrix will determine the height of
  // aurora layer.
  void UpdatePerFrameData(
      int frame, const common::OrthographicCamera& camera,
      const glm::mat4& model, const absl::optional<ClickInfo>& click_info);

  // Renders the aurora paths.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, int frame,
            absl::optional<int> selected_path_index);

 private:
  // Updates the vertex data of aurora path at 'path_index'.
  void UpdatePath(int path_index);

  // Processes user click and returns the new value of
  // 'selected_control_point_'.
  absl::optional<int> ProcessClick(float control_point_radius_object_space,
                                   const glm::mat4& proj_view_model,
                                   const glm::vec3& model_center,
                                   const absl::optional<ClickInfo>& click_info);

  // Returns the index of the clicked control point. If no control point is hit,
  // returns absl::nullopt.
  absl::optional<int> FindClickedControlPoint(
      int path_index, const glm::vec3& click_object_space,
      float control_point_radius_object_space);

  // Tries to insert a control point at the click position, and returns whether
  // the point is inserted.
  bool InsertControlPoint(int path_index, const glm::vec3& click_object_space,
                          const glm::mat4& proj_view_model,
                          const glm::vec3& model_center);

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // aurora paths does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Desired radius of each control point in the screen coordinate.
  const float control_point_radius_;

  // Number of aurora paths.
  const int num_paths_;

  // Records for each state, what color and alpha should be used when rendering
  // the viewpoint.
  const std::array<glm::vec4, button::kNumStates> viewpoint_color_alphas_;

  // Tracks the control point selected by left click.
  absl::optional<int> selected_control_point_;

  // Records for each state, what color and alpha should be used when rendering
  // the aurora path at the same index.
  std::vector<std::array<glm::vec4, button::kNumStates>> path_color_alphas_;

  // Records the color and alpha to use when rendering the aurora path at the
  // same index.
  std::vector<glm::vec4> color_alphas_to_render_;

  // Renderer of aurora paths and viewpoint of user.
  PathRenderer path_renderer_;

  // Editors of aurora paths.
  std::vector<std::unique_ptr<common::SplineEditor>> spline_editors_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_PATH_H */
