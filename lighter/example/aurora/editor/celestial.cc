//
//  celestial.cc
//
//  Created by Pujun Lun on 12/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/editor/celestial.h"

#include <array>
#include <cstdint>

#include "lighter/common/file.h"
#include "lighter/example/util.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::example::aurora {
namespace {

using renderer::extension::ModelBuilder;
using renderer::ir::AccessLocation;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;

// Consistent with uniform blocks defined in shaders.
struct EarthTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

struct TextureIndex {
  ALIGN_SCALAR(int32_t) int32_t value;
};

}  // namespace

Celestial::Celestial(const renderer::ir::Renderer& renderer,
                     float viewport_aspect_ratio, int num_frames_in_flight) {
  using common::file::GetResourcePath;
  using TextureType = ModelBuilder::TextureType;
  constexpr int kObjFileIndexBase = 1;

  const std::array uniform_usages{
      BufferUsage::GetUniformBufferUsage(AccessLocation::kVertexShader)};
  earth_uniforms_.resize(num_frames_in_flight);
  for (auto& buffer : earth_uniforms_) {
    buffer = renderer.CreateBuffer(Buffer::UpdateRate::kHigh,
                                   sizeof(EarthTrans), uniform_usages);
  }

  earth_model_ =
      ModelBuilder{renderer, "Earth", viewport_aspect_ratio,
                   ModelBuilder::SingleMeshResource{
                       GetResourcePath("model/sphere.obj"),
                       kObjFileIndexBase,
                       /*tex_source_map=*/
                       {{
                           TextureType::kDiffuse,
                           {
                               GetResourcePath("texture/earth/day.jpg"),
                               GetResourcePath("texture/earth/night.jpg"),
                           },
                       }},
                   }}
          .AddTextureBindingPoint(TextureType::kDiffuse, /*binding=*/2)
          .SetShader(renderer::ir::shader_stage::VERTEX,
                     GetShaderBinaryPath("aurora/earth.vert"))
          .SetShader(renderer::ir::shader_stage::FRAGMENT,
                     GetShaderBinaryPath("aurora/earth.frag"))
          .Build();

  const ModelBuilder::CubemapPath skybox_path{
      .directory = GetResourcePath("texture/universe/PositiveX.jpg",
                                   /*want_directory_path=*/true),
      .files = {"PositiveX.jpg", "NegativeX.jpg", "PositiveY.jpg",
                "NegativeY.jpg", "PositiveZ.jpg", "NegativeZ.jpg"},
  };

  skybox_model_ =
      ModelBuilder{renderer, "Skybox", viewport_aspect_ratio,
                   ModelBuilder::SingleMeshResource{
                       GetResourcePath("model/skybox.obj"),
                       kObjFileIndexBase,
                       {{TextureType::kCubemap, {skybox_path}}},
                   }}
          .AddTextureBindingPoint(TextureType::kCubemap, /*binding=*/1)
          .SetShader(renderer::ir::shader_stage::VERTEX,
                     GetShaderBinaryPath("shared/skybox.vert"))
          .SetShader(renderer::ir::shader_stage::FRAGMENT,
                     GetShaderBinaryPath("shared/skybox.frag"))
          .Build();
}

void Celestial::Draw(renderer::ir::PassContext& ctx,
                     const glm::ivec2& frame_size, int frame_slot,
                     EarthTextureIndex texture_index,
                     const glm::mat4& earth_proj_view_model,
                     const glm::mat4& skybox_proj_view_model) const {
  const EarthTrans earth_trans{earth_proj_view_model};
  const TextureIndex tex_index{static_cast<int32_t>(texture_index)};
  const Buffer& earth_uniform = *earth_uniforms_[frame_slot];
  earth_uniform.CopyToDevice(
      std::array{Buffer::CopyInfo{&earth_trans, sizeof(EarthTrans), 0}});

  earth_model_->BindPipeline(ctx, frame_size);
  ctx.BindBuffer(/*binding=*/0, earth_uniform);
  ctx.PushConstants(renderer::ir::shader_stage::FRAGMENT, /*offset=*/0,
                    sizeof(TextureIndex), &tex_index);
  earth_model_->Draw(ctx);

  const SkyboxTrans skybox_trans{skybox_proj_view_model};
  skybox_model_->BindPipeline(ctx, frame_size);
  ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                    sizeof(SkyboxTrans), &skybox_trans);
  skybox_model_->Draw(ctx);
}

}  // namespace lighter::example::aurora
