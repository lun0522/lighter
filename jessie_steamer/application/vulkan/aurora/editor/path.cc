//
//  celestial.cc
//
//  Created by Pujun Lun on 12/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/path.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using std::vector;

enum VertexBufferBindingPoint {
  kPathVertexBufferBindingPoint = 0,
  kColorAlphaVertexBufferBindingPoint,
};

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

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

} /* namespace */

AuroraPath::AuroraPath(const SharedBasicContext& context, int num_paths,
                       float viewport_aspect_ratio, int num_frames_in_flight)
    : num_paths_{num_paths}, viewport_aspect_ratio_{viewport_aspect_ratio},
      pipeline_builder_{context} {
  /* Vertex buffer */
  path_vertex_buffers_.reserve(num_paths_);
  for (int path = 0; path < num_paths_; ++path) {
    path_vertex_buffers_.emplace_back(absl::make_unique<DynamicPerVertexBuffer>(
        context, /*initial_size=*/1,
        pipeline::GetVertexAttribute<common::Vertex3DPosOnly>()));
  }
  color_alpha_vertex_buffer_ = absl::make_unique<DynamicPerInstanceBuffer>(
      context, sizeof(ColorAlpha), num_paths_, ColorAlpha::GetAttributes());

  /* Push constant */
  trans_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(Transformation), num_frames_in_flight);
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      trans_constant_->size_per_frame()};

  pipeline_builder_
      .SetName("aurora path")
      .SetDepthTestEnabled(/*enable_test=*/true, /*enable_write=*/false)
      .SetPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
      .AddVertexInput(
          kPathVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<common::Vertex3DPosOnly>(),
          path_vertex_buffers_[0]->GetAttributes(/*start_location=*/0))
      .AddVertexInput(
          kColorAlphaVertexBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<ColorAlpha>(),
          color_alpha_vertex_buffer_->GetAttributes(/*start_location=*/1))
      .SetPipelineLayout(/*descriptor_layouts=*/{}, {push_constant_range})
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("aurora_path.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora_path.frag"));
}

void AuroraPath::UpdateFramebuffer(
    const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
    const RenderPass& render_pass, uint32_t subpass_index) {
  pipeline_ = pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(pipeline::GetViewport(frame_size, viewport_aspect_ratio_))
      .SetRenderPass(*render_pass, subpass_index)
      .Build();
}

void AuroraPath::UpdatePath(
    int path_index, const vector<glm::vec3>& control_points,
    const vector<glm::vec3>& spline_points) {
  path_vertex_buffers_.at(path_index)->CopyHostData(
      PerVertexBuffer::NoIndicesDataInfo{
          /*per_mesh_vertices=*/{{
              PerVertexBuffer::VertexDataInfo{spline_points},
          }},
      });
}

void AuroraPath::UpdateTransMatrix(int frame, const common::Camera& camera) {
  // TODO: Pass in earth trans matrix.
  trans_constant_->HostData<Transformation>(frame)->proj_view_model =
      camera.projection() * camera.view();
}

void AuroraPath::Draw(const VkCommandBuffer& command_buffer, int frame) const {
  // TODO: Pass in color alpha for paths.
  const vector<glm::vec4> color_alphas{
      glm::vec4{1.0f, 0.0f, 0.0f, 1.0f},
      glm::vec4{0.0f, 1.0f, 0.0f, 1.0f},
      glm::vec4{0.0f, 0.0f, 1.0f, 1.0f},
  };
  color_alpha_vertex_buffer_->CopyHostData(color_alphas);

  pipeline_->Bind(command_buffer);
  trans_constant_->Flush(command_buffer, pipeline_->layout(), frame,
                         /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
  for (int path = 0; path < num_paths_; ++path) {
    color_alpha_vertex_buffer_->Bind(
        command_buffer, kColorAlphaVertexBufferBindingPoint, /*offset=*/path);
    path_vertex_buffers_[path]->Draw(
        command_buffer, kPathVertexBufferBindingPoint,
        /*mesh_index=*/0, /*instance_count=*/1);
  }
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
