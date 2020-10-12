//
//  geometry_pass.cc
//
//  Created by Pujun Lun on 5/5/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/troop/geometry_pass.h"

#include <vector>

#include "lighter/common/file.h"
#include "lighter/renderer/align.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace lighter {
namespace application {
namespace vulkan {
namespace troop {
namespace {

using namespace renderer;
using namespace renderer::vulkan;

enum SubpassIndex {
  kRenderSubpassIndex = 0,
  kNumSubpasses,
};

enum UniformBindingPoint {
  kUniformBufferBindingPoint = 0,
  kDiffuseTextureBindingPoint,
  kSpecularTextureBindingPoint,
  kReflectionTextureBindingPoint,
};

enum ColorAttachmentIndex {
  kAttachmentNonApplicable = -1,
  kPositionAttachmentIndex = 0,
  kNormalAttachmentIndex,
  kDiffuseSpecularAttachmentIndex,
};

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Transformation {
  ALIGN_MAT4 glm::mat4 model;
  ALIGN_MAT4 glm::mat4 model_inv_trs;
  ALIGN_MAT4 glm::mat4 proj_view;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

GeometryPass::GeometryPass(const WindowContext* window_context,
                           int num_frames_in_flight,
                           float model_scale, const glm::ivec2& num_soldiers,
                           const glm::vec2& interval_between_soldiers)
    : num_soldiers_{num_soldiers.x * num_soldiers.y},
      window_context_{*FATAL_IF_NULL(window_context)} {
  using TextureType = ModelBuilder::TextureType;
  const auto context = window_context_.basic_context();

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
      pipeline::GetVertexAttributes<common::Vertex3DPosOnly>());

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
      window_context_.original_aspect_ratio(),
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

void GeometryPass::UpdateFramebuffer(const Image& depth_stencil_image,
                                     const Image& position_image,
                                     const Image& normal_image,
                                     const Image& diffuse_specular_image) {
  /* Render pass */
  const Attachment attachments_to_update[]{
      {"Depth stencil", &depth_stencil_image, &depth_stencil_attachment_index_,
       kAttachmentNonApplicable},
      {"Position", &position_image, &position_color_attachment_index_,
       kPositionAttachmentIndex},
      {"Normal", &normal_image, &normal_color_attachment_index_,
       kNormalAttachmentIndex},
      {"Diffuse specular", &diffuse_specular_image,
       &diffuse_specular_color_attachment_index_,
       kDiffuseSpecularAttachmentIndex},
  };

  if (render_pass_builder_ == nullptr) {
    CreateRenderPassBuilder(attachments_to_update);
  }
  for (const auto& attachment : attachments_to_update) {
    render_pass_builder_->UpdateAttachmentImage(
        attachment.attachment_index.value(),
        [&attachment](int) -> const Image& { return attachment.image; });
  }
  render_pass_ = render_pass_builder_->Build();

  /* Model */
  nanosuit_model_->Update(/*is_object_opaque=*/true,
                          depth_stencil_image.extent(), kSingleSample,
                          *render_pass_, kRenderSubpassIndex);
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

void GeometryPass::CreateRenderPassBuilder(
    absl::Span<const Attachment> attachments) {
  // TODO: Image usage tracker should be held by upper level. Infer when to
  // preserve attachment content?
  auto depth_stencil_load_store_ops =
      GraphicsPass::GetDefaultDepthStencilLoadStoreOps();
  depth_stencil_load_store_ops.depth_store_op = VK_ATTACHMENT_STORE_OP_STORE;

  GraphicsPass graphics_pass{window_context_.basic_context(), kNumSubpasses};
  for (const auto& attachment : attachments) {
    ImageUsageHistory history{attachment.image.GetInitialUsage()};
    if (attachment.location == kAttachmentNonApplicable) {
      history.AddUsage(kRenderSubpassIndex,
                       ImageUsage::GetDepthStencilUsage(
                           ImageUsage::AccessType::kReadWrite));
      attachment.attachment_index = graphics_pass.AddAttachment(
          attachment.image_name, std::move(history), /*get_location=*/nullptr,
          depth_stencil_load_store_ops);
    } else {
      history
          .AddUsage(kRenderSubpassIndex, ImageUsage::GetRenderTargetUsage())
          .SetFinalUsage(ImageUsage::GetSampledInFragmentShaderUsage());
      attachment.attachment_index = graphics_pass.AddAttachment(
          attachment.image_name, std::move(history),
          attachment.MakeLocationGetter());
    }
  }

  render_pass_builder_ = graphics_pass.CreateRenderPassBuilder(
      /*num_framebuffers=*/window_context_.num_swapchain_images());
}

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
