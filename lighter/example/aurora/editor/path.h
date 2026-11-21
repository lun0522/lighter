#ifndef LIGHTER_EXAMPLE_AURORA_EDITOR_PATH_H
#define LIGHTER_EXAMPLE_AURORA_EDITOR_PATH_H

#include <array>
#include <cstdint>
#include <functional>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/scene/camera.h"
#include "lighter/scene/spline.h"
#include "lighter/ui/button.h"

namespace lighter::example::aurora {

// View into an uploaded aurora spline vertex buffer and its active vertex
// count.
struct SplinePathView {
  const renderer::ir::Buffer* vertex_buffer = nullptr;
  uint32_t vertex_count = 0;
};

// Renders 3D aurora splines, control point spheres, and the user viewpoint
// inside a Frame Graph graphics pass.
class PathRenderer3D {
 public:
  PathRenderer3D(const renderer::ir::Renderer& renderer,
                 int num_frames_in_flight, int num_paths);

  // This class is neither copyable nor movable.
  PathRenderer3D(const PathRenderer3D&) = delete;
  PathRenderer3D& operator=(const PathRenderer3D&) = delete;

  // Records updated control and spline points for 'path_index'.
  void UpdatePath(int path_index, std::span<const glm::vec3> control_points,
                  std::span<const glm::vec3> spline_points);

  void DrawControlPoints(renderer::ir::PassContext& ctx,
                         const glm::ivec2& frame_size,
                         float viewport_aspect_ratio, int frame_slot,
                         int path_index, float scale,
                         const glm::mat4& proj_view_model,
                         const glm::vec4& color_alpha);

  void DrawSplines(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
                   float viewport_aspect_ratio, int frame_slot,
                   const glm::mat4& proj_view_model,
                   std::span<const glm::vec4> color_alphas);

  void DrawViewpoint(renderer::ir::PassContext& ctx,
                     const glm::ivec2& frame_size, float viewport_aspect_ratio,
                     float scale, const glm::mat4& proj_view_model,
                     const glm::vec3& center,
                     const glm::vec4& color_alpha) const;

  // Ensures slot 0 holds up-to-date spline vertex data and returns views to all
  // spline buffers. Caller must ensure GPU idle if paths were recently edited.
  std::vector<SplinePathView> GetSplinePathViews();

 private:
  // Host-side copy of one path's control/spline vertices and revision number.
  struct PathHostData {
    std::vector<glm::vec3> control_points;
    std::vector<glm::vec3> spline_points;
    uint64_t version = 1;
  };

  // Per-frame-in-flight GPU buffers and synced revision for one aurora path.
  struct PathSlotBuffers {
    std::unique_ptr<renderer::ir::Buffer> control_points_buffer;
    size_t control_capacity = 0;
    std::unique_ptr<renderer::ir::Buffer> spline_points_buffer;
    size_t spline_capacity = 0;
    uint64_t synced_version = 0;
  };

  void EnsureSlotSynced(int frame_slot, int path_index);

  const renderer::ir::Renderer& renderer_;
  const int num_paths_;

  uint32_t sphere_index_count_ = 0;
  size_t sphere_index_offset_ = 0;
  std::unique_ptr<renderer::ir::Buffer> sphere_buffer_;

  std::vector<PathHostData> path_host_datas_;
  std::vector<std::vector<PathSlotBuffers>> path_slots_;
  std::vector<std::unique_ptr<renderer::ir::Buffer>> color_alpha_buffers_;

  renderer::ir::GraphicsPipelineDescriptor base_control_pipeline_;
  renderer::ir::GraphicsPipelineDescriptor base_spline_pipeline_;
  renderer::ir::GraphicsPipelineDescriptor base_viewpoint_pipeline_;
};

// Manages editable 3D aurora paths and the user viewpoint on Earth.
class AuroraPath {
 public:
  using GenerateControlPoints =
      std::function<std::vector<glm::vec3>(int path_index)>;

  // Initialization parameters, colors, and spline tessellation limits for
  // paths.
  struct Info {
    int max_num_control_points;
    float control_point_radius;
    int max_recursion_depth;
    float spline_roughness;
    glm::vec3 viewpoint_initial_pos;
    std::array<glm::vec3, ui::button::kNumStates> viewpoint_colors;
    std::span<const std::array<glm::vec3, ui::button::kNumStates>> path_colors;
    std::array<float, ui::button::kNumStates> path_alphas;
    GenerateControlPoints generate_control_points;
  };

  // Normalized object-space click event routed to path/viewpoint editing.
  struct ClickInfo {
    std::optional<int> path_index;
    bool is_left_click;
    glm::vec3 click_object_space;
  };

  AuroraPath(const renderer::ir::Renderer& renderer, int num_frames_in_flight,
             float viewport_aspect_ratio, const Info& info);

  // This class is neither copyable nor movable.
  AuroraPath(const AuroraPath&) = delete;
  AuroraPath& operator=(const AuroraPath&) = delete;

  // Processes optional click input and draws all splines, active control
  // points, and the user viewpoint into 'ctx'.
  void UpdateAndDraw(renderer::ir::PassContext& ctx,
                     const glm::ivec2& frame_size, int frame_slot,
                     const scene::OrthographicCamera& camera,
                     const glm::mat4& model, float model_radius,
                     std::optional<int> selected_path_index,
                     const std::optional<ClickInfo>& click_info);

  std::vector<SplinePathView> GetSplinePathViews() {
    return path_renderer_.GetSplinePathViews();
  }

  const glm::vec3& viewpoint_position() const { return viewpoint_pos_; }

 private:
  void UpdatePath(int path_index);

  std::optional<int> ProcessClick(float control_point_radius_object_space,
                                  const glm::mat4& proj_view_model,
                                  const glm::vec3& model_center,
                                  const std::optional<ClickInfo>& click_info);

  std::optional<int> FindClickedControlPoint(
      int path_index, const glm::vec3& click_object_space,
      float control_point_radius_object_space) const;

  bool InsertControlPoint(int path_index, const glm::vec3& click_object_space,
                          const glm::mat4& proj_view_model,
                          const glm::vec3& model_center);

  const float viewport_aspect_ratio_;
  const float control_point_radius_;
  const int num_paths_;
  const std::array<glm::vec4, ui::button::kNumStates> viewpoint_color_alphas_;

  std::optional<int> selected_control_point_;
  glm::vec3 viewpoint_pos_;
  bool did_click_viewpoint_ = false;
  std::vector<std::array<glm::vec4, ui::button::kNumStates>> path_color_alphas_;
  std::vector<glm::vec4> color_alphas_to_render_;

  PathRenderer3D path_renderer_;
  std::vector<std::unique_ptr<scene::SplineEditor>> spline_editors_;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_EDITOR_PATH_H
