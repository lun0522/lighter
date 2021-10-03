//
//  path.cc
//
//  Created by Pujun Lun on 12/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/editor/path.h"

#include "lighter/application/vulkan/util.h"
#include "lighter/common/data.h"
#include "lighter/common/file.h"
#include "lighter/renderer/util.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace renderer::vulkan;

enum class ControlVertexBufferBindingPoint { kCenter = 0, kPos };

enum class SplineVertexBufferBindingPoint { kPos = 0, kColorAlpha };

constexpr uint32_t kViewpointVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct ColorAlpha {
  // Returns vertex input attributes.
  static std::vector<common::VertexAttribute> GetVertexAttributes() {
    return common::data::CreateVertexAttributes<glm::vec4>();
  }

  glm::vec4 value;
};

/* END: Consistent with vertex input attributes defined in shaders. */

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct ControlRenderInfo {
  ALIGN_MAT4 glm::mat4 proj_view_model;
  ALIGN_VEC4 glm::vec4 color_alpha;
  ALIGN_SCALAR(float) float scale;
};

struct SplineTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

struct ViewpointRenderInfo {
  ALIGN_MAT4 glm::mat4 proj_view_model;
  ALIGN_VEC4 glm::vec4 color_alpha;
  ALIGN_VEC4 glm::vec4 center_scale;
};

/* END: Consistent with uniform blocks defined in shaders. */

// Applies 'transform' to a 3D 'point'.
inline glm::vec3 TransformPoint(const glm::mat4& transform,
                                const glm::vec3& point) {
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed} / transformed.w;
}

} /* namespace */

PathRenderer3D::PathRenderer3D(const SharedBasicContext& context,
                               int num_frames_in_flight, int num_paths)
    : num_paths_{num_paths}, num_control_points_per_path_(num_paths_),
      control_pipeline_builder_{context}, spline_pipeline_builder_{context},
      viewpoint_pipeline_builder_{context} {
  using common::Vertex3DPosOnly;

  // Prevent shaders from being auto released.
  ShaderModule::AutoReleaseShaderPool shader_pool;

  /* Vertex buffer */
  const common::ObjFilePosOnly sphere_file{
      common::file::GetResourcePath("model/small_sphere.obj"),
      /*index_base=*/1};
  PerVertexBuffer::NoShareIndicesDataInfo sphere_vertices_info{
      /*per_mesh_infos=*/{{
          PerVertexBuffer::VertexDataInfo{sphere_file.indices},
          PerVertexBuffer::VertexDataInfo{sphere_file.vertices},
      }},
  };
  sphere_vertex_buffer_ = std::make_unique<StaticPerVertexBuffer>(
      context, std::move(sphere_vertices_info),
      pipeline::GetVertexAttributes<Vertex3DPosOnly>());

  paths_vertex_buffers_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    paths_vertex_buffers_.push_back(PathVertexBuffers{
        std::make_unique<DynamicPerInstanceBuffer>(
            context, sizeof(Vertex3DPosOnly), /*max_num_instances=*/1,
            pipeline::GetVertexAttributes<Vertex3DPosOnly>()),
        std::make_unique<DynamicPerVertexBuffer>(
            context, /*initial_size=*/1,
            pipeline::GetVertexAttributes<Vertex3DPosOnly>()),
    });
  }

  color_alpha_vertex_buffer_ = std::make_unique<DynamicPerInstanceBuffer>(
      context, sizeof(ColorAlpha), num_paths_,
      pipeline::GetVertexAttributes<ColorAlpha>());

  /* Push constant */
  control_render_constant_ = std::make_unique<PushConstant>(
      context, sizeof(ControlRenderInfo), num_frames_in_flight);
  spline_trans_constant_ = std::make_unique<PushConstant>(
      context, sizeof(SplineTrans), num_frames_in_flight);
  viewpoint_render_constant_ = std::make_unique<PushConstant>(
      context, sizeof(ViewpointRenderInfo), num_frames_in_flight);

  /* Pipeline */
  control_pipeline_builder_
      .SetPipelineName("Aurora path control")
      .SetDepthTestEnable(/*enable_test=*/true, /*enable_write=*/false)
      .AddVertexInput(
          static_cast<uint32_t>(ControlVertexBufferBindingPoint::kCenter),
          pipeline::GetPerInstanceBindingDescription<Vertex3DPosOnly>(),
          paths_vertex_buffers_[0].spline_points_buffer
              ->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          static_cast<uint32_t>(ControlVertexBufferBindingPoint::kPos),
          pipeline::GetPerVertexBindingDescription<Vertex3DPosOnly>(),
          sphere_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {control_render_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetColorBlend({pipeline::GetColorAlphaBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("aurora/draw_path_control.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("aurora/draw_path.frag"));

  spline_pipeline_builder_
      .SetPipelineName("Aurora path spline")
      .SetDepthTestEnable(/*enable_test=*/true, /*enable_write=*/false)
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
      .AddVertexInput(
          static_cast<uint32_t>(SplineVertexBufferBindingPoint::kPos),
          pipeline::GetPerVertexBindingDescription<Vertex3DPosOnly>(),
          paths_vertex_buffers_[0].spline_points_buffer
              ->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          static_cast<uint32_t>(SplineVertexBufferBindingPoint::kColorAlpha),
          pipeline::GetPerInstanceBindingDescription<ColorAlpha>(),
          color_alpha_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {spline_trans_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetColorBlend({pipeline::GetColorAlphaBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("aurora/draw_path_spline.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("aurora/draw_path.frag"));

  viewpoint_pipeline_builder_
      .SetPipelineName("User viewpoint")
      .SetDepthTestEnable(/*enable_test=*/true, /*enable_write=*/false)
      .AddVertexInput(
          kViewpointVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<Vertex3DPosOnly>(),
          sphere_vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(/*descriptor_layouts=*/{},
                         {viewpoint_render_constant_->MakePerFrameRange(
                             VK_SHADER_STAGE_VERTEX_BIT)})
      .SetColorBlend({pipeline::GetColorAlphaBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("aurora/viewpoint.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("aurora/draw_path.frag"));
}

void PathRenderer3D::UpdatePath(int path_index,
                                absl::Span<const glm::vec3> control_points,
                                absl::Span<const glm::vec3> spline_points) {
  num_control_points_per_path_[path_index] = control_points.size();
  paths_vertex_buffers_[path_index].control_points_buffer->CopyHostData(
      control_points);
  paths_vertex_buffers_[path_index].spline_points_buffer->CopyHostData(
      PerVertexBuffer::NoIndicesDataInfo{
          /*per_mesh_vertices=*/{{
              PerVertexBuffer::VertexDataInfo{spline_points},
          }},
      });
}

void PathRenderer3D::UpdateFramebuffer(
    VkSampleCountFlagBits sample_count,
    const RenderPass& render_pass, uint32_t subpass_index,
    const GraphicsPipelineBuilder::ViewportInfo& viewport) {
  // Prevent shaders from being auto released.
  ShaderModule::AutoReleaseShaderPool shader_pool;

  control_pipeline_ = control_pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(GraphicsPipelineBuilder::ViewportInfo{viewport})
      .SetRenderPass(*render_pass, subpass_index)
      .Build();
  spline_pipeline_ = spline_pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(GraphicsPipelineBuilder::ViewportInfo{viewport})
      .SetRenderPass(*render_pass, subpass_index)
      .Build();
  viewpoint_pipeline_ = viewpoint_pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(GraphicsPipelineBuilder::ViewportInfo{viewport})
      .SetRenderPass(*render_pass, subpass_index)
      .Build();
}

void PathRenderer3D::UpdatePerFrameData(int frame, float control_point_scale,
                                        const glm::mat4& proj_view_model) {
  // 'color_alpha' will be updated by DrawControlPoints().
  auto& control_render_info =
      *control_render_constant_->HostData<ControlRenderInfo>(frame);
  control_render_info.proj_view_model = proj_view_model;
  control_render_info.scale = control_point_scale;

  spline_trans_constant_->HostData<SplineTrans>(frame)->proj_view_model =
      proj_view_model;

  // 'color_alpha' and 'center_scale' will be updated by DrawViewpoint().
  viewpoint_render_constant_->HostData<ViewpointRenderInfo>(frame)
                            ->proj_view_model
      = proj_view_model;
}

void PathRenderer3D::DrawControlPoints(
    const VkCommandBuffer& command_buffer, int frame,
    int path_index, const glm::vec4& color_alpha) {
  control_render_constant_->HostData<ControlRenderInfo>(frame)->color_alpha =
      color_alpha;
  control_pipeline_->Bind(command_buffer);
  control_render_constant_->Flush(command_buffer, control_pipeline_->layout(),
                                  frame, /*target_offset=*/0,
                                  VK_SHADER_STAGE_VERTEX_BIT);
  paths_vertex_buffers_[path_index].control_points_buffer->Bind(
      command_buffer,
      static_cast<uint32_t>(ControlVertexBufferBindingPoint::kCenter),
      /*offset=*/0);
  sphere_vertex_buffer_->Draw(
      command_buffer,
      static_cast<uint32_t>(ControlVertexBufferBindingPoint::kPos),
      /*mesh_index=*/0, num_control_points_per_path_[path_index]);
}

void PathRenderer3D::DrawSplines(
    const VkCommandBuffer& command_buffer, int frame,
    absl::Span<const glm::vec4> color_alphas) {
  ASSERT_TRUE(color_alphas.size() == num_paths_,
              absl::StrFormat("Length of 'color_alphas' (%d) must match with "
                              "the number of aurora paths (%d)",
                              color_alphas.size(), num_paths_));
  color_alpha_vertex_buffer_->CopyHostData(color_alphas);
  spline_pipeline_->Bind(command_buffer);
  spline_trans_constant_->Flush(command_buffer, spline_pipeline_->layout(),
                                frame, /*target_offset=*/0,
                                VK_SHADER_STAGE_VERTEX_BIT);
  for (int path = 0; path < num_paths_; ++path) {
    color_alpha_vertex_buffer_->Bind(
        command_buffer,
        static_cast<uint32_t>(SplineVertexBufferBindingPoint::kColorAlpha),
        /*offset=*/path);
    paths_vertex_buffers_[path].spline_points_buffer->Draw(
        command_buffer,
        static_cast<uint32_t>(SplineVertexBufferBindingPoint::kPos),
        /*mesh_index=*/0, /*instance_count=*/1);
  }
}

void PathRenderer3D::DrawViewpoint(
    const VkCommandBuffer& command_buffer, int frame,
    const glm::vec3& center, const glm::vec4& color_alpha) {
  const float scale =
      control_render_constant_->HostData<ControlRenderInfo>(frame)->scale;
  auto& render_info =
      *viewpoint_render_constant_->HostData<ViewpointRenderInfo>(frame);
  render_info.color_alpha = color_alpha;
  render_info.center_scale = {center, scale};

  viewpoint_pipeline_->Bind(command_buffer);
  viewpoint_render_constant_->Flush(
      command_buffer, viewpoint_pipeline_->layout(), frame, /*target_offset=*/0,
      VK_SHADER_STAGE_VERTEX_BIT);
  sphere_vertex_buffer_->Draw(
      command_buffer, kViewpointVertexBufferBindingPoint,
      /*mesh_index=*/0, /*instance_count=*/1);
}

std::vector<const PerVertexBuffer*>
PathRenderer3D::GetPathVertexBuffers() const {
  std::vector<const PerVertexBuffer*> buffers;
  buffers.reserve(num_paths_);
  for (const auto& buffer : paths_vertex_buffers_) {
    buffers.push_back(buffer.spline_points_buffer.get());
  }
  return buffers;
}

AuroraPath::AuroraPath(const SharedBasicContext& context,
                       int num_frames_in_flight, float viewport_aspect_ratio,
                       const Info& info)
    : viewport_aspect_ratio_{viewport_aspect_ratio},
      control_point_radius_{info.control_point_radius},
      num_paths_{static_cast<int>(info.path_colors.size())},
      viewpoint_color_alphas_{
          glm::vec4{info.viewpoint_colors[button::kSelectedState],
                    info.path_alphas[button::kSelectedState]},
          glm::vec4{info.viewpoint_colors[button::kUnselectedState],
                    info.path_alphas[button::kUnselectedState]}},
      viewpoint_pos_{info.viewpoint_initial_pos},
      color_alphas_to_render_(num_paths_),
      path_renderer_{context, num_frames_in_flight, num_paths_} {
  path_color_alphas_.reserve(num_paths_);
  spline_editors_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    path_color_alphas_.push_back(std::array<glm::vec4, button::kNumStates>{
        glm::vec4{info.path_colors[path][button::kSelectedState],
                  info.path_alphas[button::kSelectedState]},
        glm::vec4{info.path_colors[path][button::kUnselectedState],
                  info.path_alphas[button::kUnselectedState]},
    });
    spline_editors_.push_back(std::make_unique<common::SplineEditor>(
        common::CatmullRomSpline::kMinNumControlPoints,
        info.max_num_control_points, info.generate_control_points(path),
        common::CatmullRomSpline::GetOnSphereSpline(
            info.max_recursion_depth, info.spline_roughness)));
    UpdatePath(path);
  }
}

void AuroraPath::UpdateFramebuffer(
    const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
    const RenderPass& render_pass, uint32_t subpass_index) {
  path_renderer_.UpdateFramebuffer(
      sample_count, render_pass, subpass_index,
      pipeline::GetViewport(frame_size, viewport_aspect_ratio_));
}

void AuroraPath::UpdatePerFrameData(
    int frame, const common::OrthographicCamera& camera,
    const glm::mat4& model, float model_radius,
    const std::optional<ClickInfo>& click_info) {
  const float radius_object_space = camera.view_width() * control_point_radius_;
  const float control_point_scale = radius_object_space / model_radius;
  const glm::mat4 proj_view_model = camera.GetProjectionMatrix() *
                                    camera.GetViewMatrix() * model;
  path_renderer_.UpdatePerFrameData(frame, control_point_scale,
                                    proj_view_model);
  selected_control_point_ = ProcessClick(radius_object_space, proj_view_model,
                                         /*model_center=*/model[3], click_info);
}

void AuroraPath::Draw(const VkCommandBuffer& command_buffer, int frame,
                      std::optional<int> selected_path_index) {
  if (selected_path_index.has_value()) {
    ASSERT_TRUE(selected_path_index.value() < num_paths_,
                absl::StrFormat("Path index (%d) out of range (%d)",
                                selected_path_index.value(), num_paths_));
  }

  // If one path is selected, highlight it. Otherwise, highlight all paths.
  if (selected_path_index.has_value()) {
    for (int path = 0; path < num_paths_; ++path) {
      color_alphas_to_render_[path] =
          path_color_alphas_[path][button::kUnselectedState];
    }
    color_alphas_to_render_[selected_path_index.value()] =
        path_color_alphas_[selected_path_index.value()][button::kSelectedState];
  } else {
    for (int path = 0; path < num_paths_; ++path) {
      color_alphas_to_render_[path] =
          path_color_alphas_[path][button::kSelectedState];
    }
  }
  path_renderer_.DrawSplines(command_buffer, frame, color_alphas_to_render_);

  // Render controls points only if one path is selected.
  if (selected_path_index.has_value()) {
    path_renderer_.DrawControlPoints(
        command_buffer, frame, selected_path_index.value(),
        color_alphas_to_render_[selected_path_index.value()]);
  }

  // Render user viewpoint at last.
  const bool is_viewpoint_selected = !selected_path_index.has_value();
  const auto viewpoint_state = is_viewpoint_selected ? button::kSelectedState
                                                     : button::kUnselectedState;
  path_renderer_.DrawViewpoint(command_buffer, frame, viewpoint_pos_,
                               viewpoint_color_alphas_[viewpoint_state]);
}

void AuroraPath::UpdatePath(int path_index) {
  path_renderer_.UpdatePath(path_index,
                            spline_editors_[path_index]->control_points(),
                            spline_editors_[path_index]->spline_points());
}

std::optional<int> AuroraPath::ProcessClick(
    float control_point_radius_object_space,
    const glm::mat4& proj_view_model, const glm::vec3& model_center,
    const std::optional<ClickInfo>& click_info) {
  if (!click_info.has_value()) {
    did_click_viewpoint_ = false;
    return std::nullopt;
  }

  // If no 'path_index' specified, process click on viewpoint.
  const ClickInfo& user_click = click_info.value();
  if (!click_info.value().path_index.has_value()) {
    // If keep clicking on the viewpoint, or right click for the first time,
    // simply move the viewpoint to click point.
    if (did_click_viewpoint_ || !user_click.is_left_click) {
      viewpoint_pos_ = user_click.click_object_space;
    }
    // If left click on the viewpoint or right click anywhere on the earth
    // model for the first time, start to track clicking.
    if (!did_click_viewpoint_) {
      const bool is_left_clicking_on_viewpoint =
          user_click.is_left_click &&
          glm::distance(viewpoint_pos_, user_click.click_object_space) <=
              control_point_radius_object_space;
      if (is_left_clicking_on_viewpoint || !user_click.is_left_click) {
        did_click_viewpoint_ = true;
      }
    }
    return std::nullopt;
  }

  // Otherwise, process click on aurora paths.
  const int path_index = user_click.path_index.value();
  ASSERT_TRUE(path_index < num_paths_,
              absl::StrFormat(
                  "Trying to access aurora path at index %d (%d paths exist)",
                  path_index, num_paths_));

  // If a control point has been selected before this frame, simply move it to
  // the current click point.
  auto& editor = *spline_editors_[path_index];
  if (selected_control_point_.has_value() && user_click.is_left_click) {
    editor.UpdateControlPoint(selected_control_point_.value(),
                              user_click.click_object_space);
    UpdatePath(path_index);
    return selected_control_point_;
  }

  const auto clicked_control_point = FindClickedControlPoint(
      path_index, user_click.click_object_space,
      control_point_radius_object_space);
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
            : InsertControlPoint(path_index, user_click.click_object_space,
                                 proj_view_model, model_center);
    if (is_path_changed) {
      UpdatePath(path_index);
    }
    return std::nullopt;
  }
}

std::optional<int> AuroraPath::FindClickedControlPoint(
    int path_index, const glm::vec3& click_object_space,
    float control_point_radius_object_space) {
  const auto& control_points =
      spline_editors_[path_index]->control_points();
  for (int i = 0; i < control_points.size(); ++i) {
    if (glm::distance(control_points[i], click_object_space) <=
        control_point_radius_object_space) {
      return i;
    }
  }
  return std::nullopt;
}

bool AuroraPath::InsertControlPoint(
    int path_index, const glm::vec3& click_object_space,
    const glm::mat4& proj_view_model, const glm::vec3& model_center) {
  auto& editor = *spline_editors_[path_index];
  if (!editor.CanInsertControlPoint()) {
    return false;
  }

  const auto& control_points = editor.control_points();
  const float model_center_depth =
      TransformPoint(proj_view_model, model_center).z;
  const glm::vec2 click_pos_ndc =
      TransformPoint(proj_view_model, click_object_space);

  // Find the closest visible control point.
  struct ClosestPoint {
    int index;
    glm::vec2 pos_ndc;
  };
  std::optional<ClosestPoint> closest_control_point;
  for (int i = 0; i < control_points.size(); i++) {
    const glm::vec3 control_point_ndc =
        TransformPoint(proj_view_model, control_points[i]);
    // If the depth of this control point is no less than the depth of earth
    // center, it must be invisible from the current viewpoint.
    if (control_point_ndc.z >= model_center_depth) {
      continue;
    }
    if (!closest_control_point.has_value() ||
        glm::distance(click_pos_ndc, glm::vec2{control_point_ndc}) <
            glm::distance(click_pos_ndc, closest_control_point->pos_ndc)) {
      closest_control_point = ClosestPoint{i, control_point_ndc};
    }
  }

  // Check adjacent control points and pick the one closer to the click point.
  // Since adjacent points may be invisible, we simply use 3D distance.
  if (closest_control_point.has_value()) {
    const int prev_point_index = static_cast<int>(
        (closest_control_point->index - 1) % control_points.size());
    const int next_point_index = static_cast<int>(
        (closest_control_point->index + 1) % control_points.size());
    const float prev_point_distance =
        glm::distance(control_points[prev_point_index], click_object_space);
    const float next_point_distance =
        glm::distance(control_points[next_point_index], click_object_space);
    const int insert_at_index =
        prev_point_distance < next_point_distance ? closest_control_point->index
                                                  : next_point_index;
    return editor.InsertControlPoint(insert_at_index, click_object_space);
  }
  return false;
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
