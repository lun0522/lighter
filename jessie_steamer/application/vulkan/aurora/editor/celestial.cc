//
//  celestial.cc
//
//  Created by Pujun Lun on 12/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/celestial.h"

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct EarthTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

struct TextureIndex {
  ALIGN_SCALAR(int32_t) int32_t value;
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace */

Celestial::Celestial(const SharedBasicContext& context,
                     float viewport_aspect_ratio, int num_frames_in_flight)
    : viewport_aspect_ratio_{viewport_aspect_ratio} {
  using common::file::GetResourcePath;
  using common::file::GetVkShaderPath;
  using TextureType = ModelBuilder::TextureType;
  constexpr int kObjFileIndexBase = 1;

  earth_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(EarthTrans), num_frames_in_flight);
  earth_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(TextureIndex), num_frames_in_flight);
  skybox_constant_ = absl::make_unique<PushConstant>(
      context, sizeof(SkyboxTrans), num_frames_in_flight);

  earth_model_ = ModelBuilder{
      context, "earth", num_frames_in_flight, viewport_aspect_ratio_,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/sphere.obj"), kObjFileIndexBase,
          /*tex_source_map=*/{{
              TextureType::kDiffuse, {
                  SharedTexture::SingleTexPath{
                      GetResourcePath("texture/earth/day.jpg")},
                  SharedTexture::SingleTexPath{
                      GetResourcePath("texture/earth/night.jpg")},
              },
          }}
      }}
      .AddTextureBindingPoint(TextureType::kDiffuse, /*binding_point=*/2)
      .AddUniformBinding(
          VK_SHADER_STAGE_VERTEX_BIT,
          /*bindings=*/{{/*binding_point=*/0, /*array_length=*/1}})
      .AddUniformBuffer(/*binding_point=*/0, *earth_uniform_)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT)
      .AddPushConstant(earth_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("earth.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("earth.frag"))
      .Build();

  const SharedTexture::CubemapPath skybox_path{
      /*directory=*/GetResourcePath("texture/universe"),
      /*files=*/{
          "PositiveX.jpg", "NegativeX.jpg",
          "PositiveY.jpg", "NegativeY.jpg",
          "PositiveZ.jpg", "NegativeZ.jpg",
      },
  };

  skybox_model_ = ModelBuilder{
      context, "skybox", num_frames_in_flight, viewport_aspect_ratio_,
      ModelBuilder::SingleMeshResource{
          GetResourcePath("model/skybox.obj"), kObjFileIndexBase,
          {{TextureType::kCubemap, {skybox_path}}},
      }}
      .AddTextureBindingPoint(TextureType::kCubemap, /*binding_point=*/1)
      .SetPushConstantShaderStage(VK_SHADER_STAGE_VERTEX_BIT)
      .AddPushConstant(skybox_constant_.get(), /*target_offset=*/0)
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT, GetVkShaderPath("skybox.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT, GetVkShaderPath("skybox.frag"))
      .Build();
}

void Celestial::UpdateFramebuffer(
    const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
    const RenderPass& render_pass, uint32_t subpass_index) {
  constexpr bool kIsObjectOpaque = true;
  earth_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                       render_pass, subpass_index);
  skybox_model_->Update(kIsObjectOpaque, frame_size, sample_count,
                        render_pass, subpass_index);
}

void Celestial::UpdateEarthData(int frame, EarthTextureIndex texture_index,
                                const glm::mat4& proj_view_model) {
  earth_constant_->HostData<TextureIndex>(frame)->value = texture_index;
  earth_uniform_->HostData<EarthTrans>(frame)->proj_view_model =
      proj_view_model;
  earth_uniform_->Flush(frame);
}

void Celestial::UpdateSkyboxData(int frame, const glm::mat4& proj_view_model) {
  skybox_constant_->HostData<SkyboxTrans>(frame)->proj_view_model =
      proj_view_model;
}

void Celestial::Draw(const VkCommandBuffer& command_buffer, int frame) const {
  earth_model_->Draw(command_buffer, frame, /*instance_count=*/1);
  skybox_model_->Draw(command_buffer, frame, /*instance_count=*/1);
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
