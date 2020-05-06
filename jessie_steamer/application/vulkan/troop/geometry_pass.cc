//
//  geometry_pass.cc
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/troop/geometry_pass.h"

#include <vector>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace troop {
namespace {

using namespace wrapper::vulkan;

enum UniformBindingPoint {
  kUniformBufferBindingPoint = 0,
  kDiffuseTextureBindingPoint,
  kSpecularTextureBindingPoint,
  kReflectionTextureBindingPoint,
};

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 model_inv_trs;
  ALIGN_MAT4 glm::mat4 proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

GeometryPass::GeometryPass(const SharedBasicContext& context,
                           int num_frames_in_flight,
                           float viewport_aspect_ratio,
                           float model_scale, const glm::ivec2& num_soldiers,
                           const glm::vec2& interval_between_soldiers)
    : num_soldiers_{num_soldiers.x * num_soldiers.y} {
  using TextureType = ModelBuilder::TextureType;

  /* Vertex buffer */
  std::vector<glm::vec3> centers;
  centers.reserve(num_soldiers_);
  for (int x = 0; x < num_soldiers.x; ++x) {
    for (int z = 0; z < num_soldiers.y; ++z) {
      centers.push_back(glm::vec3{
          interval_between_soldiers.x * static_cast<float>(x), 0.0f,
          interval_between_soldiers.y * static_cast<float>(z),
      });
    }
  }
  center_data_ = absl::make_unique<StaticPerInstanceBuffer>(
      context, centers,
      pipeline::GetVertexAttribute<common::Vertex3DPosOnly>());

  /* Uniform buffer */
  trans_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(Transformation), num_frames_in_flight);
  const glm::mat4 model = glm::scale(glm::mat4{1.0f}, glm::vec3{model_scale});
  const glm::mat4 model_inv_trs = glm::transpose(glm::inverse(model));
  for (int frame = 0; frame < num_frames_in_flight; ++frame) {
    auto& trans = *trans_uniform_->HostData<Transformation>(frame);
    trans.model = model;
    trans.model_inv_trs = model_inv_trs;
    constexpr auto kDataSize = sizeof(Transformation::model) +
                               sizeof(Transformation::model_inv_trs);
    trans_uniform_->Flush(frame, kDataSize, offsetof(Transformation, model));
  }

  /* Model */
  nanosuit_model_ = ModelBuilder{
      context, "Nanosuit", num_frames_in_flight, viewport_aspect_ratio,
      ModelBuilder::MultiMeshResource{
          common::file::GetResourcePath("model/nanosuit/nanosuit.obj"),
          common::file::GetResourcePath("model/nanosuit")}}
      .AddTextureBindingPoint(TextureType::kDiffuse,
                              kDiffuseTextureBindingPoint)
      .AddTextureBindingPoint(TextureType::kSpecular,
                              kSpecularTextureBindingPoint)
      .AddTextureBindingPoint(TextureType::kReflection,
                              kReflectionTextureBindingPoint)
      .AddPerInstanceBuffer(center_data_.get())
      .AddUniformBinding(
          VK_SHADER_STAGE_VERTEX_BIT,
          /*bindings=*/{{kUniformBufferBindingPoint, /*array_length=*/1}})
      .AddUniformBuffer(kUniformBufferBindingPoint, *trans_uniform_)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("troop/geometry_pass.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("troop/geometry_pass.frag"))
      .Build();
}

void GeometryPass::UpdateFramebuffer(const VkExtent2D& frame_size,
                                     const RenderPass& render_pass,
                                     uint32_t subpass_index) {
  nanosuit_model_->Update(/*is_object_opaque=*/true, frame_size,
                          kSingleSample, render_pass, subpass_index);
}

void GeometryPass::UpdatePerFrameData(int frame, const common::Camera& camera) {
  trans_uniform_->HostData<Transformation>(frame)->proj_view =
      camera.GetProjectionMatrix() * camera.GetViewMatrix();
  trans_uniform_->Flush(frame, sizeof(Transformation::proj_view),
                        offsetof(Transformation, proj_view));
}

void GeometryPass::Draw(const VkCommandBuffer& command_buffer,
                        int frame) const {
  nanosuit_model_->Draw(command_buffer, frame,
                        /*instance_count=*/num_soldiers_);
}

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
