//
//  geometry_pass.cc
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/troop/geometry_pass.h"

#include <utility>
#include <vector>

#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/align.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/glm/gtc/matrix_transform.hpp"

namespace lighter {
namespace application {
namespace vulkan {
namespace troop {
namespace {

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
  const std::pair<const Image*, AttachmentInfo*> attachments_to_update[]{
      {&depth_stencil_image, &depth_stencil_image_info},
      {&position_image, &position_image_info},
      {&normal_image, &normal_image_info},
      {&diffuse_specular_image, &diffuse_specular_image_info},
  };

  if (render_pass_builder_ == nullptr) {
    image::UsageTracker image_usage_tracker;
    for (const auto& pair : attachments_to_update) {
      pair.second->AddToTracker(image_usage_tracker, *pair.first);
    }

    GraphicsPass graphics_pass{window_context_.basic_context(), kNumSubpasses};
    attachments_to_update[0].second->AddToGraphicsPass(
        graphics_pass, image_usage_tracker,
        /*populate_history=*/[](image::UsageHistory& history) {
          history.AddUsage(kRenderSubpassIndex,
                           image::Usage::GetDepthStencilUsage());
        });
    for (int i = 1; i < 4; ++i) {
      attachments_to_update[i].second->AddToGraphicsPass(
          graphics_pass, image_usage_tracker,
          /*populate_history=*/[](image::UsageHistory& history) {
            history
                .AddUsage(kRenderSubpassIndex,
                          image::Usage::GetRenderTargetUsage())
                .SetFinalUsage(image::Usage::GetSampledInFragmentShaderUsage());
          });
    }
    render_pass_builder_ = graphics_pass.CreateRenderPassBuilder(
        /*num_framebuffers=*/window_context_.num_swapchain_images());
  }

  for (const auto& pair : attachments_to_update) {
    render_pass_builder_->UpdateAttachmentImage(
        pair.second->index(),
        [&pair](int framebuffer_index) -> const Image& { return *pair.first; });
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

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
