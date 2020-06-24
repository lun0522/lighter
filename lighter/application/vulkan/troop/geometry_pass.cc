//
//  geometry_pass.cc
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/troop/geometry_pass.h"

#include <vector>

#include "lighter/common/file.h"
#include "lighter/wrapper/vulkan/align.h"
#include "lighter/wrapper/vulkan/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace lighter {
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

enum ColorAttachmentIndex {
  kPositionImageIndex = 0,
  kNormalImageIndex,
  kDiffuseSpecularImageIndex,
  kNumColorAttachments,
};

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 model_inv_trs;
  ALIGN_MAT4 glm::mat4 proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

GeometryPass::GeometryPass(const WindowContext& window_context,
                           int num_frames_in_flight,
                           float model_scale, const glm::ivec2& num_soldiers,
                           const glm::vec2& interval_between_soldiers)
    : num_soldiers_{num_soldiers.x * num_soldiers.y} {
  using TextureType = ModelBuilder::TextureType;
  const auto context = window_context.basic_context();

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
      context, "Geometry pass", num_frames_in_flight,
      window_context.original_aspect_ratio(),
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

  /* Render pass */
  render_pass_builder_ = absl::make_unique<DeferredShadingRenderPassBuilder>(
      context, /*num_framebuffers=*/window_context.num_swapchain_images(),
      kNumColorAttachments);
}

void GeometryPass::UpdateFramebuffer(const Image& depth_stencil_image,
                                     const Image& position_image,
                                     const Image& normal_image,
                                     const Image& diffuse_specular_image) {
  /* Render pass */
  const int color_attachments_index_base =
      render_pass_builder_->color_attachments_index_base();
  (*render_pass_builder_)
      .UpdateAttachmentImage(
          render_pass_builder_->depth_stencil_attachment_index(),
          [&depth_stencil_image](int framebuffer_index) -> const Image& {
            return depth_stencil_image;
          })
      .UpdateAttachmentImage(
          color_attachments_index_base + kPositionImageIndex,
          [&position_image](int framebuffer_index) -> const Image& {
            return position_image;
          })
      .UpdateAttachmentImage(
          color_attachments_index_base + kNormalImageIndex,
          [&normal_image](int framebuffer_index) -> const Image& {
            return normal_image;
          })
      .UpdateAttachmentImage(
          color_attachments_index_base + kDiffuseSpecularImageIndex,
          [&diffuse_specular_image](int framebuffer_index) -> const Image& {
            return diffuse_specular_image;
          });
  render_pass_ = render_pass_builder_->Build();

  /* Model */
  nanosuit_model_->Update(/*is_object_opaque=*/true,
                          depth_stencil_image.extent(), kSingleSample,
                          *render_pass_, /*subpass_index=*/0);
}

void GeometryPass::UpdatePerFrameData(int frame, const common::Camera& camera) {
  trans_uniform_->HostData<Transformation>(frame)->proj_view =
      camera.GetProjectionMatrix() * camera.GetViewMatrix();
  trans_uniform_->Flush(frame, sizeof(Transformation::proj_view),
                        offsetof(Transformation, proj_view));
}

void GeometryPass::Draw(const VkCommandBuffer& command_buffer,
                        uint32_t framebuffer_index, int current_frame) const {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
          [this, current_frame](const VkCommandBuffer& command_buffer) {
            nanosuit_model_->Draw(command_buffer, current_frame,
                                  /*instance_count=*/num_soldiers_);
          },
      });
}

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
