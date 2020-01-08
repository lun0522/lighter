//
//  path.cc
//
//  Created by Pujun Lun on 12/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/path.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using std::array;
using std::vector;

enum class ControlVertexBufferBindingPoint { kCenter = 0, kPos };

enum class SplineVertexBufferBindingPoint { kPos = 0, kColorAlpha };

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct ColorAlpha {
  static vector<VertexBuffer::Attribute> GetAttributes() {
    return {{offsetof(ColorAlpha, value), VK_FORMAT_R32G32B32A32_SFLOAT}};
  }

  glm::vec4 value;
};

/* END: Consistent with vertex input attributes defined in shaders. */

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct ControlRenderInfo {
  ALIGN_MAT4 glm::mat4 proj_view_model;
  ALIGN_VEC4 glm::vec4 color_alpha;
  ALIGN_SCALAR(float) float radius;
};

struct SplineTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Extracts the position data from a list of Vertex3DWithTex.
vector<common::Vertex3DPosOnly> ExtractPos(
    const vector<common::Vertex3DWithTex>& vertices) {
  vector<common::Vertex3DPosOnly> vertices_pos;
  vertices_pos.reserve(vertices.size());
  for (const auto& vertex : vertices) {
    vertices_pos.emplace_back(vertex.pos);
  }
  return vertices_pos;
}

} /* namespace */

AuroraPath::AuroraPath(const SharedBasicContext& context,
                       int num_frames_in_flight, float viewport_aspect_ratio,
                       const Info& info)
    : viewport_aspect_ratio_{viewport_aspect_ratio},
      control_point_radius_{info.control_point_radius},
      num_paths_{static_cast<int>(info.path_colors.size())},
      num_control_points_(num_paths_), color_alphas_to_render_(num_paths_),
      control_pipeline_builder_{context},
      spline_pipeline_builder_{context} {
  using common::file::GetVkShaderPath;
  using common::Vertex3DPosOnly;

  /* Vertex buffer */
  path_color_alphas_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    path_color_alphas_.emplace_back(array<glm::vec4, state::kNumStates>{
        glm::vec4{info.path_colors[path][state::kSelected],
                  info.path_alphas[state::kSelected]},
        glm::vec4{info.path_colors[path][state::kUnselected],
                  info.path_alphas[state::kUnselected]},
    });
  }

  const common::ObjFile sphere_file{
      common::file::GetResourcePath("model/small_sphere.obj"),
      /*index_base=*/1};
  const auto sphere_vertices = ExtractPos(sphere_file.vertices);
  PerVertexBuffer::NoShareIndicesDataInfo sphere_vertices_info{
      /*per_mesh_infos=*/{{
          PerVertexBuffer::VertexDataInfo{sphere_file.indices},
          PerVertexBuffer::VertexDataInfo{sphere_vertices},
      }},
  };
  sphere_vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context, std::move(sphere_vertices_info),
      pipeline::GetVertexAttribute<Vertex3DPosOnly>());

  paths_vertex_buffers_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    paths_vertex_buffers_.emplace_back(PathVertexBuffers{
        absl::make_unique<DynamicPerInstanceBuffer>(
            context, sizeof(Vertex3DPosOnly), /*max_num_instances=*/1,
            pipeline::GetVertexAttribute<Vertex3DPosOnly>()),
        absl::make_unique<DynamicPerVertexBuffer>(
            context, /*initial_size=*/1,
            pipeline::GetVertexAttribute<Vertex3DPosOnly>()),
    });
  }

  color_alpha_vertex_buffer_ = absl::make_unique<DynamicPerInstanceBuffer>(
      context, sizeof(ColorAlpha), num_paths_, ColorAlpha::GetAttributes());

  /* Push constant */
  control_render_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(ControlRenderInfo), num_frames_in_flight);
  const VkPushConstantRange control_render_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      control_render_constant_->size_per_frame()};

  spline_trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(SplineTrans), num_frames_in_flight);
  const VkPushConstantRange spline_trans_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      spline_trans_constant_->size_per_frame()};

  /* Pipeline */
  control_pipeline_builder_
      .SetName("aurora path control")
      .SetDepthTestEnabled(/*enable_test=*/true, /*enable_write=*/false)
      .AddVertexInput(
          static_cast<int>(ControlVertexBufferBindingPoint::kCenter),
          pipeline::GetPerInstanceBindingDescription<Vertex3DPosOnly>(),
          paths_vertex_buffers_[0].spline_points_buffer
              ->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          static_cast<int>(ControlVertexBufferBindingPoint::kPos),
          pipeline::GetPerVertexBindingDescription<Vertex3DPosOnly>(),
          sphere_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {control_render_constant_range})
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetVkShaderPath("spline_3d_control.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("spline.frag"));

  spline_pipeline_builder_
      .SetName("aurora path spline")
      .SetDepthTestEnabled(/*enable_test=*/true, /*enable_write=*/false)
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
      .AddVertexInput(
          static_cast<int>(SplineVertexBufferBindingPoint::kPos),
          pipeline::GetPerVertexBindingDescription<Vertex3DPosOnly>(),
          paths_vertex_buffers_[0].spline_points_buffer
              ->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          static_cast<int>(SplineVertexBufferBindingPoint::kColorAlpha),
          pipeline::GetPerInstanceBindingDescription<ColorAlpha>(),
          color_alpha_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {spline_trans_constant_range})
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("spline_3d.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("spline.frag"));

  /* Path editor */
  spline_editors_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    spline_editors_.emplace_back(absl::make_unique<common::SplineEditor>(
        common::CatmullRomSpline::kMinNumControlPoints,
        info.max_num_control_points, info.generate_control_points(path),
        common::CatmullRomSpline::GetOnSphereSpline(
            info.max_recursion_depth, info.spline_smoothness)));
    UpdatePath(path);
  }
}

void AuroraPath::UpdateFramebuffer(
    const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
    const RenderPass& render_pass, uint32_t subpass_index) {
  control_pipeline_ = control_pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(pipeline::GetViewport(frame_size, viewport_aspect_ratio_))
      .SetRenderPass(*render_pass, subpass_index)
      .Build();
  spline_pipeline_ = spline_pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(pipeline::GetViewport(frame_size, viewport_aspect_ratio_))
      .SetRenderPass(*render_pass, subpass_index)
      .Build();
}

void AuroraPath::UpdatePerFrameData(
    int frame, const common::OrthographicCamera& camera,
    const glm::mat4& model, const absl::optional<ClickInfo>& click_info) {
  control_render_constant_
      ->HostData<ControlRenderInfo>(frame)->proj_view_model =
  spline_trans_constant_->HostData<SplineTrans>(frame)->proj_view_model =
      camera.projection() * camera.view() * model;

  constexpr float kSphereModelRadius = 1.0f;
  const float desired_radius = camera.view_width() * control_point_radius_;
  control_render_constant_->HostData<ControlRenderInfo>(frame)->radius =
      desired_radius / kSphereModelRadius;
  selected_control_point_ = ProcessClick(desired_radius, click_info);
}

void AuroraPath::UpdatePath(int path_index) {
  const auto& editor = *spline_editors_[path_index];
  num_control_points_[path_index] = editor.control_points().size();
  paths_vertex_buffers_[path_index].control_points_buffer->CopyHostData(
      editor.control_points());
  paths_vertex_buffers_[path_index].spline_points_buffer->CopyHostData(
      PerVertexBuffer::NoIndicesDataInfo{
          /*per_mesh_vertices=*/{{
              PerVertexBuffer::VertexDataInfo{editor.spline_points()},
          }},
      });
}

absl::optional<int> AuroraPath::ProcessClick(
    float control_point_radius, const absl::optional<ClickInfo>& click_info) {
  if (!click_info.has_value()) {
    return absl::nullopt;
  }

  const ClickInfo& user_click = click_info.value();
  if (user_click.path_index >= num_paths_) {
    FATAL(absl::StrFormat(
        "Trying to access aurora path at index %d (%d paths exist)",
        user_click.path_index, num_paths_));
  }

  // If a control point has been selected before this frame, simply move it to
  // the current click point.
  auto& editor = *spline_editors_[user_click.path_index];
  if (selected_control_point_.has_value() && user_click.is_left_click) {
    editor.UpdateControlPoint(selected_control_point_.value(),
                              user_click.click_object_space);
    UpdatePath(user_click.path_index);
    return selected_control_point_;
  }

  const auto clicked_control_point = editor.FindClickedControlPoint(
      user_click.click_object_space, control_point_radius);
  if (user_click.is_left_click) {
    // For left click, if no control point has been selected, find out if any
    // control point is selected in this frame.
    return clicked_control_point;
  } else {
    // For right click, if any control point is clicked, remove it. Otherwise,
    // add a new control point at the click point.
    const bool is_path_changed =
        clicked_control_point.has_value()
            ? editor.RemoveControlPoint(clicked_control_point.value())
            : editor.AddControlPoint(user_click.click_object_space,
                                     /*max_distance_from_spline=*/
                                     control_point_radius);
    if (is_path_changed) {
      UpdatePath(user_click.path_index);
    }
    return absl::nullopt;
  }
}

void AuroraPath::Draw(const VkCommandBuffer& command_buffer, int frame,
                      absl::optional<int> selected_path_index) {
  // If one path is selected, highlight it. Otherwise, highlight all paths.
  if (selected_path_index.has_value()) {
    for (int path = 0; path < num_paths_; ++path) {
      color_alphas_to_render_[path] =
          path_color_alphas_[path][state::kUnselected];
    }
    const int selected_path = selected_path_index.value();
    color_alphas_to_render_[selected_path] =
        path_color_alphas_[selected_path][state::kSelected];
  } else {
    for (int path = 0; path < num_paths_; ++path) {
      color_alphas_to_render_[path] =
          path_color_alphas_[path][state::kSelected];
    }
  }
  color_alpha_vertex_buffer_->CopyHostData(color_alphas_to_render_);

  spline_pipeline_->Bind(command_buffer);
  spline_trans_constant_->Flush(
      command_buffer, spline_pipeline_->layout(), frame, /*target_offset=*/0,
      VK_SHADER_STAGE_VERTEX_BIT);
  for (int path = 0; path < num_paths_; ++path) {
    color_alpha_vertex_buffer_->Bind(
        command_buffer,
        static_cast<int>(SplineVertexBufferBindingPoint::kColorAlpha),
        /*offset=*/path);
    paths_vertex_buffers_[path].spline_points_buffer->Draw(
        command_buffer, static_cast<int>(SplineVertexBufferBindingPoint::kPos),
        /*mesh_index=*/0, /*instance_count=*/1);
  }

  // Render controls points only if one path is selected.
  if (!selected_path_index.has_value()) {
    return;
  }

  const int selected_path = selected_path_index.value();
  control_render_constant_->HostData<ControlRenderInfo>(frame)->color_alpha =
      path_color_alphas_[selected_path][state::kSelected];

  control_pipeline_->Bind(command_buffer);
  control_render_constant_->Flush(
      command_buffer, control_pipeline_->layout(), frame, /*target_offset=*/0,
      VK_SHADER_STAGE_VERTEX_BIT);
  paths_vertex_buffers_[selected_path].control_points_buffer->Bind(
      command_buffer,
      static_cast<int>(ControlVertexBufferBindingPoint::kCenter), /*offset=*/0);
  sphere_vertex_buffer_->Draw(
      command_buffer, static_cast<int>(ControlVertexBufferBindingPoint::kPos),
      /*mesh_index=*/0, num_control_points_[selected_path]);
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
