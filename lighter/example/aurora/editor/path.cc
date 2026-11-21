#include "lighter/example/aurora/editor/path.h"

#include <algorithm>
#include <cstddef>
#include <format>
#include <glm/gtc/matrix_transform.hpp>

#include "lighter/base/data.h"
#include "lighter/base/file.h"
#include "lighter/base/util.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/buffer_util.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/scene/util.h"

namespace lighter::example::aurora {
namespace {

using base::Vertex3DPosOnly;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::DataFormat;
using renderer::ir::GraphicsPipelineDescriptor;
using renderer::ir::PassContext;
using renderer::ir::PrimitiveTopology;
using renderer::ir::Renderer;
using renderer::ir::VertexBufferView;
using renderer::ir::VertexInputRate;

enum class ControlVertexBufferBindingPoint { kCenter = 0, kPos };
enum class SplineVertexBufferBindingPoint { kPos = 0, kColorAlpha };
constexpr uint32_t kViewpointVertexBufferBindingPoint = 0;

// Consistent with uniform blocks defined in shaders.
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

glm::vec3 TransformPoint(const glm::mat4& transform, const glm::vec3& point) {
  const glm::vec4 transformed = transform * glm::vec4{point, 1.0f};
  return glm::vec3{transformed} / transformed.w;
}

GraphicsPipelineDescriptor WithViewport(const GraphicsPipelineDescriptor& base,
                                        const glm::ivec2& frame_size,
                                        float viewport_aspect_ratio) {
  GraphicsPipelineDescriptor desc = base;
  desc.SetViewport(
      renderer::ir::pipeline::GetViewport(frame_size, viewport_aspect_ratio),
      renderer::ir::pipeline::GetFullFrameScissor(frame_size), /*flip_y=*/true);
  return desc;
}

}  // namespace

PathRenderer3D::PathRenderer3D(const Renderer& renderer,
                               int num_frames_in_flight, int num_paths)
    : renderer_{renderer},
      num_paths_{num_paths},
      path_host_datas_(static_cast<size_t>(num_paths)) {
  const base::ObjFilePosOnly sphere_file{
      base::file::GetResourcePath("model/small_sphere.obj"),
      /*index_base=*/1};
  sphere_index_count_ = static_cast<uint32_t>(sphere_file.indices.size());
  const size_t sphere_vertex_bytes = base::util::GetTotalDataSize(
      std::span<const Vertex3DPosOnly>{sphere_file.vertices});
  const size_t sphere_index_bytes = base::util::GetTotalDataSize(
      std::span<const uint32_t>{sphere_file.indices});
  sphere_index_offset_ = sphere_vertex_bytes;

  sphere_buffer_ = renderer_.CreateBuffer(
      Buffer::UpdateRate::kLow, sphere_vertex_bytes + sphere_index_bytes,
      std::array{BufferUsage::GetVertexBufferUsage(
          BufferUsage::UsageType::kIndexAndVertex)});
  sphere_buffer_->CopyToDevice(std::array{
      Buffer::CopyInfo{sphere_file.vertices.data(), sphere_vertex_bytes, 0},
      Buffer::CopyInfo{sphere_file.indices.data(), sphere_index_bytes,
                       sphere_index_offset_},
  });

  path_slots_.resize(static_cast<size_t>(num_frames_in_flight));
  for (auto& per_path : path_slots_) {
    per_path.resize(static_cast<size_t>(num_paths_));
  }

  const std::array vertex_usages{
      BufferUsage::GetVertexBufferUsage(BufferUsage::UsageType::kVertexOnly)};
  color_alpha_buffers_.resize(static_cast<size_t>(num_frames_in_flight));
  for (auto& buffer : color_alpha_buffers_) {
    buffer = renderer_.CreateBuffer(
        Buffer::UpdateRate::kHigh,
        sizeof(glm::vec4) * static_cast<size_t>(num_paths_), vertex_usages);
  }

  const auto alpha_blend = renderer::ir::pipeline::GetColorAlphaBlend();

  base_control_pipeline_.SetName("AuroraPathControl")
      .SetShader(
          renderer::ir::shader_stage::VERTEX,
          base::file::GetShaderBinaryPath("aurora/draw_path_control.vert"))
      .SetShader(renderer::ir::shader_stage::FRAGMENT,
                 base::file::GetShaderBinaryPath("aurora/draw_path.frag"))
      .EnableDepthTestOnly()
      .UseColorAttachment(/*location=*/0, alpha_blend)
      .AddVertexInput({
          VertexInputRate::kInstance,
          static_cast<int>(ControlVertexBufferBindingPoint::kCenter),
          sizeof(glm::vec3),
          renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(0),
      })
      .AddVertexInput({
          VertexInputRate::kVertex,
          static_cast<int>(ControlVertexBufferBindingPoint::kPos),
          sizeof(Vertex3DPosOnly),
          renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(1),
      });

  base_spline_pipeline_.SetName("AuroraPathSpline")
      .SetShader(
          renderer::ir::shader_stage::VERTEX,
          base::file::GetShaderBinaryPath("aurora/draw_path_spline.vert"))
      .SetShader(renderer::ir::shader_stage::FRAGMENT,
                 base::file::GetShaderBinaryPath("aurora/draw_path.frag"))
      .EnableDepthTestOnly()
      .SetPrimitiveTopology(PrimitiveTopology::kLineStrip)
      .UseColorAttachment(/*location=*/0, alpha_blend)
      .AddVertexInput({
          VertexInputRate::kVertex,
          static_cast<int>(SplineVertexBufferBindingPoint::kPos),
          sizeof(glm::vec3),
          renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(0),
      })
      .AddVertexInput({
          VertexInputRate::kInstance,
          static_cast<int>(SplineVertexBufferBindingPoint::kColorAlpha),
          sizeof(glm::vec4),
          {{/*location=*/1, DataFormat::kSFloat32Vec4, /*offset=*/0}},
      });

  base_viewpoint_pipeline_.SetName("UserViewpoint")
      .SetShader(renderer::ir::shader_stage::VERTEX,
                 base::file::GetShaderBinaryPath("aurora/viewpoint.vert"))
      .SetShader(renderer::ir::shader_stage::FRAGMENT,
                 base::file::GetShaderBinaryPath("aurora/draw_path.frag"))
      .EnableDepthTestOnly()
      .UseColorAttachment(/*location=*/0, alpha_blend)
      .AddVertexInput({
          VertexInputRate::kVertex,
          kViewpointVertexBufferBindingPoint,
          sizeof(Vertex3DPosOnly),
          renderer::ir::buffer::CreateAttributesForVertex3DPosOnly(0),
      });
}

void PathRenderer3D::UpdatePath(int path_index,
                                std::span<const glm::vec3> control_points,
                                std::span<const glm::vec3> spline_points) {
  PathHostData& data = path_host_datas_.at(static_cast<size_t>(path_index));
  data.control_points.assign(control_points.begin(), control_points.end());
  data.spline_points.assign(spline_points.begin(), spline_points.end());
  ++data.version;
}

void PathRenderer3D::EnsureSlotSynced(int frame_slot, int path_index) {
  const PathHostData& data =
      path_host_datas_.at(static_cast<size_t>(path_index));
  PathSlotBuffers& slot = path_slots_[static_cast<size_t>(frame_slot)]
                                     [static_cast<size_t>(path_index)];
  if (slot.synced_version == data.version) {
    return;
  }

  const std::array vertex_usages{
      BufferUsage::GetVertexBufferUsage(BufferUsage::UsageType::kVertexOnly)};
  if (data.control_points.size() > slot.control_capacity) {
    slot.control_capacity =
        std::max(data.control_points.size(), slot.control_capacity * 2);
    slot.control_points_buffer = renderer_.CreateBuffer(
        Buffer::UpdateRate::kHigh, sizeof(glm::vec3) * slot.control_capacity,
        vertex_usages);
  }
  if (!data.control_points.empty()) {
    slot.control_points_buffer->CopyToDevice(std::array{Buffer::CopyInfo{
        data.control_points.data(),
        base::util::GetTotalDataSize(
            std::span<const glm::vec3>{data.control_points}),
        /*offset=*/0,
    }});
  }

  if (data.spline_points.size() > slot.spline_capacity) {
    slot.spline_capacity =
        std::max(data.spline_points.size(), slot.spline_capacity * 2);
    slot.spline_points_buffer = renderer_.CreateBuffer(
        Buffer::UpdateRate::kHigh, sizeof(glm::vec3) * slot.spline_capacity,
        vertex_usages);
  }
  if (!data.spline_points.empty()) {
    slot.spline_points_buffer->CopyToDevice(std::array{Buffer::CopyInfo{
        data.spline_points.data(),
        base::util::GetTotalDataSize(
            std::span<const glm::vec3>{data.spline_points}),
        /*offset=*/0,
    }});
  }

  slot.synced_version = data.version;
}

void PathRenderer3D::DrawControlPoints(
    PassContext& ctx, const glm::ivec2& frame_size, float viewport_aspect_ratio,
    int frame_slot, int path_index, float scale,
    const glm::mat4& proj_view_model, const glm::vec4& color_alpha) {
  EnsureSlotSynced(frame_slot, path_index);
  const size_t num_points =
      path_host_datas_[static_cast<size_t>(path_index)].control_points.size();
  if (num_points == 0) {
    return;
  }

  constexpr uint32_t kControlPushConstantSize =
      static_cast<uint32_t>(offsetof(ControlRenderInfo, scale) + sizeof(float));
  const ControlRenderInfo info{proj_view_model, color_alpha, scale};
  ctx.BindGraphicsPipeline(
      WithViewport(base_control_pipeline_, frame_size, viewport_aspect_ratio));
  ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                    kControlPushConstantSize, &info);
  ctx.BindVertexBuffer(
      static_cast<uint32_t>(ControlVertexBufferBindingPoint::kCenter),
      *path_slots_[static_cast<size_t>(frame_slot)]
                  [static_cast<size_t>(path_index)]
                      .control_points_buffer);
  ctx.BindVertexBuffer(
      static_cast<uint32_t>(ControlVertexBufferBindingPoint::kPos),
      *sphere_buffer_);
  ctx.BindIndexBuffer(*sphere_buffer_, sphere_index_offset_,
                      /*is_uint16=*/false);
  ctx.DrawIndexed(sphere_index_count_, static_cast<uint32_t>(num_points));
}

void PathRenderer3D::DrawSplines(PassContext& ctx, const glm::ivec2& frame_size,
                                 float viewport_aspect_ratio, int frame_slot,
                                 const glm::mat4& proj_view_model,
                                 std::span<const glm::vec4> color_alphas) {
  ASSERT_TRUE(color_alphas.size() == static_cast<size_t>(num_paths_),
              std::format("Length of 'color_alphas' ({}) must match with "
                          "the number of aurora paths ({})",
                          color_alphas.size(), num_paths_));

  const Buffer& color_buffer =
      *color_alpha_buffers_[static_cast<size_t>(frame_slot)];
  color_buffer.CopyToDevice(std::array{Buffer::CopyInfo{
      color_alphas.data(), base::util::GetTotalDataSize(color_alphas), 0}});

  const SplineTrans trans{proj_view_model};
  ctx.BindGraphicsPipeline(
      WithViewport(base_spline_pipeline_, frame_size, viewport_aspect_ratio));
  ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                    sizeof(SplineTrans), &trans);

  for (int path = 0; path < num_paths_; ++path) {
    EnsureSlotSynced(frame_slot, path);
    const size_t num_vertices =
        path_host_datas_[static_cast<size_t>(path)].spline_points.size();
    if (num_vertices == 0) {
      continue;
    }
    ctx.BindVertexBuffer(
        static_cast<uint32_t>(SplineVertexBufferBindingPoint::kPos),
        *path_slots_[static_cast<size_t>(frame_slot)][static_cast<size_t>(path)]
             .spline_points_buffer);
    ctx.BindVertexBuffer(
        static_cast<uint32_t>(SplineVertexBufferBindingPoint::kColorAlpha),
        color_buffer, sizeof(glm::vec4) * static_cast<size_t>(path));
    ctx.Draw(static_cast<uint32_t>(num_vertices));
  }
}

void PathRenderer3D::DrawViewpoint(PassContext& ctx,
                                   const glm::ivec2& frame_size,
                                   float viewport_aspect_ratio, float scale,
                                   const glm::mat4& proj_view_model,
                                   const glm::vec3& center,
                                   const glm::vec4& color_alpha) const {
  const ViewpointRenderInfo info{
      proj_view_model,
      color_alpha,
      glm::vec4{center, scale},
  };
  ctx.BindGraphicsPipeline(WithViewport(base_viewpoint_pipeline_, frame_size,
                                        viewport_aspect_ratio));
  ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                    sizeof(ViewpointRenderInfo), &info);
  ctx.BindVertexBuffer(kViewpointVertexBufferBindingPoint, *sphere_buffer_);
  ctx.BindIndexBuffer(*sphere_buffer_, sphere_index_offset_,
                      /*is_uint16=*/false);
  ctx.DrawIndexed(sphere_index_count_);
}

std::vector<SplinePathView> PathRenderer3D::GetSplinePathViews() {
  std::vector<SplinePathView> views;
  views.reserve(static_cast<size_t>(num_paths_));
  for (int path = 0; path < num_paths_; ++path) {
    EnsureSlotSynced(/*frame_slot=*/0, path);
    views.push_back(SplinePathView{
        .vertex_buffer = path_slots_[0][static_cast<size_t>(path)]
                             .spline_points_buffer.get(),
        .vertex_count = static_cast<uint32_t>(
            path_host_datas_[static_cast<size_t>(path)].spline_points.size()),
    });
  }
  return views;
}

AuroraPath::AuroraPath(const Renderer& renderer, int num_frames_in_flight,
                       float viewport_aspect_ratio, const Info& info)
    : viewport_aspect_ratio_{viewport_aspect_ratio},
      control_point_radius_{info.control_point_radius},
      num_paths_{static_cast<int>(info.path_colors.size())},
      viewpoint_color_alphas_{
          glm::vec4{info.viewpoint_colors[ui::button::kSelectedState],
                    info.path_alphas[ui::button::kSelectedState]},
          glm::vec4{info.viewpoint_colors[ui::button::kUnselectedState],
                    info.path_alphas[ui::button::kUnselectedState]}},
      viewpoint_pos_{info.viewpoint_initial_pos},
      color_alphas_to_render_(static_cast<size_t>(num_paths_)),
      path_renderer_{renderer, num_frames_in_flight, num_paths_} {
  path_color_alphas_.reserve(static_cast<size_t>(num_paths_));
  spline_editors_.reserve(static_cast<size_t>(num_paths_));
  for (int path = 0; path < num_paths_; ++path) {
    path_color_alphas_.push_back(std::array<glm::vec4, ui::button::kNumStates>{
        glm::vec4{info.path_colors[static_cast<size_t>(path)]
                                  [ui::button::kSelectedState],
                  info.path_alphas[ui::button::kSelectedState]},
        glm::vec4{info.path_colors[static_cast<size_t>(path)]
                                  [ui::button::kUnselectedState],
                  info.path_alphas[ui::button::kUnselectedState]},
    });
    spline_editors_.push_back(std::make_unique<scene::SplineEditor>(
        scene::CatmullRomSpline::kMinNumControlPoints,
        info.max_num_control_points, info.generate_control_points(path),
        scene::CatmullRomSpline::GetOnSphereSpline(info.max_recursion_depth,
                                                   info.spline_roughness)));
    UpdatePath(path);
  }
}

void AuroraPath::UpdateAndDraw(PassContext& ctx, const glm::ivec2& frame_size,
                               int frame_slot,
                               const scene::OrthographicCamera& camera,
                               const glm::mat4& model, float model_radius,
                               std::optional<int> selected_path_index,
                               const std::optional<ClickInfo>& click_info) {
  if (selected_path_index.has_value()) {
    ASSERT_TRUE(selected_path_index.value() < num_paths_,
                std::format("Path index ({}) out of range ({})",
                            selected_path_index.value(), num_paths_));
  }

  const float radius_object_space = camera.view_width() * control_point_radius_;
  const float control_point_scale = radius_object_space / model_radius;
  const glm::mat4 proj_view_model =
      camera.GetProjectionMatrix() * camera.GetViewMatrix() * model;
  selected_control_point_ = ProcessClick(radius_object_space, proj_view_model,
                                         /*model_center=*/model[3], click_info);

  if (selected_path_index.has_value()) {
    for (int path = 0; path < num_paths_; ++path) {
      color_alphas_to_render_[static_cast<size_t>(path)] =
          path_color_alphas_[static_cast<size_t>(path)]
                            [ui::button::kUnselectedState];
    }
    const size_t idx = static_cast<size_t>(selected_path_index.value());
    color_alphas_to_render_[idx] =
        path_color_alphas_[idx][ui::button::kSelectedState];
  } else {
    for (int path = 0; path < num_paths_; ++path) {
      color_alphas_to_render_[static_cast<size_t>(path)] =
          path_color_alphas_[static_cast<size_t>(path)]
                            [ui::button::kSelectedState];
    }
  }

  path_renderer_.DrawSplines(ctx, frame_size, viewport_aspect_ratio_,
                             frame_slot, proj_view_model,
                             color_alphas_to_render_);

  if (selected_path_index.has_value()) {
    path_renderer_.DrawControlPoints(
        ctx, frame_size, viewport_aspect_ratio_, frame_slot,
        selected_path_index.value(), control_point_scale, proj_view_model,
        color_alphas_to_render_[static_cast<size_t>(
            selected_path_index.value())]);
  }

  const bool is_viewpoint_selected = !selected_path_index.has_value();
  const auto viewpoint_state = is_viewpoint_selected
                                   ? ui::button::kSelectedState
                                   : ui::button::kUnselectedState;
  path_renderer_.DrawViewpoint(ctx, frame_size, viewport_aspect_ratio_,
                               control_point_scale, proj_view_model,
                               viewpoint_pos_,
                               viewpoint_color_alphas_[viewpoint_state]);
}

void AuroraPath::UpdatePath(int path_index) {
  const auto& editor = *spline_editors_[static_cast<size_t>(path_index)];
  path_renderer_.UpdatePath(path_index, editor.control_points(),
                            editor.spline_points());
}

std::optional<int> AuroraPath::ProcessClick(
    float control_point_radius_object_space, const glm::mat4& proj_view_model,
    const glm::vec3& model_center, const std::optional<ClickInfo>& click_info) {
  if (!click_info.has_value()) {
    did_click_viewpoint_ = false;
    return std::nullopt;
  }

  const ClickInfo& user_click = click_info.value();
  if (!user_click.path_index.has_value()) {
    if (did_click_viewpoint_ || !user_click.is_left_click) {
      viewpoint_pos_ = user_click.click_object_space;
    }
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

  const int path_index = user_click.path_index.value();
  ASSERT_TRUE(
      path_index < num_paths_,
      std::format("Trying to access aurora path at index {} ({} paths exist)",
                  path_index, num_paths_));

  auto& editor = *spline_editors_[static_cast<size_t>(path_index)];
  if (selected_control_point_.has_value() && user_click.is_left_click) {
    editor.UpdateControlPoint(selected_control_point_.value(),
                              user_click.click_object_space);
    UpdatePath(path_index);
    return selected_control_point_;
  }

  const auto clicked_control_point =
      FindClickedControlPoint(path_index, user_click.click_object_space,
                              control_point_radius_object_space);
  if (user_click.is_left_click) {
    return clicked_control_point;
  }
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

std::optional<int> AuroraPath::FindClickedControlPoint(
    int path_index, const glm::vec3& click_object_space,
    float control_point_radius_object_space) const {
  const auto& control_points =
      spline_editors_[static_cast<size_t>(path_index)]->control_points();
  for (size_t i = 0; i < control_points.size(); ++i) {
    if (glm::distance(control_points[i], click_object_space) <=
        control_point_radius_object_space) {
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

bool AuroraPath::InsertControlPoint(int path_index,
                                    const glm::vec3& click_object_space,
                                    const glm::mat4& proj_view_model,
                                    const glm::vec3& model_center) {
  auto& editor = *spline_editors_[static_cast<size_t>(path_index)];
  if (!editor.CanInsertControlPoint()) {
    return false;
  }

  const float model_center_depth =
      TransformPoint(proj_view_model, model_center).z;
  const auto is_point_visible = [&proj_view_model,
                                 model_center_depth](const glm::vec3& p) {
    return TransformPoint(proj_view_model, p).z < model_center_depth;
  };
  const std::optional<int> insert_at_index =
      editor.FindClosestInsertionIndex(click_object_space, is_point_visible);
  return insert_at_index.has_value() &&
         editor.InsertControlPoint(*insert_at_index, click_object_space);
}

}  // namespace lighter::example::aurora
