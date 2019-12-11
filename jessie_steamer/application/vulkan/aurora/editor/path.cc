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
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using std::vector;

enum class ControlVertexBufferBindingPoint { kCenter = 0, kPos, kColorAlpha };

enum class SplineVertexBufferBindingPoint { kPos = 0, kColorAlpha };

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct PosOrCenter {
  static vector<VertexBuffer::Attribute> GetAttributes() {
    return {{offsetof(PosOrCenter, value), VK_FORMAT_R32G32B32_SFLOAT}};
  }

  glm::vec3 value;
};

struct ColorAlpha {
  static vector<VertexBuffer::Attribute> GetAttributes() {
    return {{offsetof(ColorAlpha, value), VK_FORMAT_R32G32B32A32_SFLOAT}};
  }

  glm::vec4 value;
};

/* END: Consistent with vertex input attributes defined in shaders. */

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

/* END: Consistent with uniform blocks defined in shaders. */

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

AuroraPath::AuroraPath(const SharedBasicContext& context, int num_paths,
                       float viewport_aspect_ratio, int num_frames_in_flight)
    : num_paths_{num_paths}, viewport_aspect_ratio_{viewport_aspect_ratio},
      num_control_points_(num_paths_), control_pipeline_builder_{context},
      spline_pipeline_builder_{context} {
  using common::file::GetVkShaderPath;

  /* Vertex buffer */
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
      pipeline::GetVertexAttribute<common::Vertex3DPosOnly>());

  paths_vertex_buffers_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    paths_vertex_buffers_.emplace_back(PathVertexBuffers{
        absl::make_unique<DynamicPerInstanceBuffer>(
            context, sizeof(PosOrCenter), /*max_num_instances=*/1,
            pipeline::GetVertexAttribute<common::Vertex3DPosOnly>()),
        absl::make_unique<DynamicPerVertexBuffer>(
            context, /*initial_size=*/1,
            pipeline::GetVertexAttribute<common::Vertex3DPosOnly>()),
    });
  }

  color_alpha_vertex_buffer_ = absl::make_unique<DynamicPerInstanceBuffer>(
      context, sizeof(ColorAlpha), num_paths_, ColorAlpha::GetAttributes());

  /* Push constant */
  control_trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(Transformation), num_frames_in_flight);
  spline_trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(Transformation), num_frames_in_flight);
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      spline_trans_constant_->size_per_frame()};

  /* Pipeline */
  control_pipeline_builder_
      .SetName("aurora path control")
      .SetDepthTestEnabled(/*enable_test=*/true, /*enable_write=*/false)
      .AddVertexInput(
          static_cast<int>(ControlVertexBufferBindingPoint::kCenter),
          pipeline::GetPerInstanceBindingDescription<common::Vertex3DPosOnly>(),
          paths_vertex_buffers_[0].spline_points_buffer
              ->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          static_cast<int>(ControlVertexBufferBindingPoint::kPos),
          pipeline::GetPerVertexBindingDescription<common::Vertex3DPosOnly>(),
          sphere_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .AddVertexInput(
          static_cast<int>(ControlVertexBufferBindingPoint::kColorAlpha),
          pipeline::GetPerInstanceBindingDescription<ColorAlpha>(),
          color_alpha_vertex_buffer_->GetAttributes(/*start_location=*/2))
      .SetPipelineLayout(/*descriptor_layouts=*/{}, {push_constant_range})
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
          pipeline::GetPerVertexBindingDescription<common::Vertex3DPosOnly>(),
          paths_vertex_buffers_[0].spline_points_buffer
              ->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          static_cast<int>(SplineVertexBufferBindingPoint::kColorAlpha),
          pipeline::GetPerInstanceBindingDescription<ColorAlpha>(),
          color_alpha_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .SetPipelineLayout(/*descriptor_layouts=*/{}, {push_constant_range})
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("spline_3d.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("spline.frag"));
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

void AuroraPath::UpdatePath(
    int path_index, const vector<glm::vec3>& control_points,
    const vector<glm::vec3>& spline_points) {
  num_control_points_.at(path_index) = control_points.size();
  paths_vertex_buffers_.at(path_index).control_points_buffer->CopyHostData(
      control_points);
  paths_vertex_buffers_.at(path_index).spline_points_buffer->CopyHostData(
      PerVertexBuffer::NoIndicesDataInfo{
          /*per_mesh_vertices=*/{{
              PerVertexBuffer::VertexDataInfo{spline_points},
          }},
      });
}

void AuroraPath::UpdateTransMatrix(int frame, const common::Camera& camera,
                                   const glm::mat4& model) {
  control_trans_constant_->HostData<Transformation>(frame)->proj_view_model =
  spline_trans_constant_->HostData<Transformation>(frame)->proj_view_model =
      camera.projection() * camera.view() * model;
}

void AuroraPath::Draw(const VkCommandBuffer& command_buffer, int frame) const {
  // TODO: Pass in color alpha for paths.
  const vector<glm::vec4> color_alphas{
      glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
      glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
      glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
  };
  color_alpha_vertex_buffer_->CopyHostData(color_alphas);

  control_pipeline_->Bind(command_buffer);
  control_trans_constant_->Flush(
      command_buffer, control_pipeline_->layout(), frame, /*target_offset=*/0,
      VK_SHADER_STAGE_VERTEX_BIT);
  for (int path = 0; path < num_paths_; ++path) {
    // TODO: color_alpha should be the same to all control points on one path
    color_alpha_vertex_buffer_->Bind(
        command_buffer,
        static_cast<int>(ControlVertexBufferBindingPoint::kColorAlpha),
        /*offset=*/path);
    paths_vertex_buffers_[path].control_points_buffer->Bind(
        command_buffer,
        static_cast<int>(ControlVertexBufferBindingPoint::kCenter),
        /*offset=*/0);
    sphere_vertex_buffer_->Draw(
        command_buffer, static_cast<int>(ControlVertexBufferBindingPoint::kPos),
        /*mesh_index=*/0, num_control_points_[path]);
  }

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
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
