//
//  skybox.cc
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#include "lighter/scene/skybox.h"

#include "lighter/base/data.h"
#include "lighter/base/file.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::scene {
namespace {

constexpr int kObjFileIndexBase = 1;

// Consistent with uniform blocks defined in shaders.
struct SkyboxTrans {
  ALIGN_MAT4 glm::mat4 proj_view_model;
};

}  // namespace

Skybox::Skybox(const renderer::ir::Renderer& renderer,
               float viewport_aspect_ratio,
               const ModelBuilder::CubemapPath& cubemap_path) {
  using TextureType = ModelBuilder::TextureType;
  model_ =
      ModelBuilder{
          renderer, "Skybox", viewport_aspect_ratio,
          ModelBuilder::SingleMeshResource{
              base::file::GetResourcePath("model/skybox.obj"),
              kObjFileIndexBase,
              /*tex_source_map=*/{{TextureType::kCubemap, {cubemap_path}}},
          }}
          .AddTextureBindingPoint(TextureType::kCubemap, /*binding=*/1)
          .SetShader(renderer::ir::shader_stage::VERTEX,
                     base::file::GetShaderBinaryPath("shared/skybox.vert"))
          .SetShader(renderer::ir::shader_stage::FRAGMENT,
                     base::file::GetShaderBinaryPath("shared/skybox.frag"))
          .Build();
}

void Skybox::Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
                  const glm::mat4& proj_view_model) const {
  const SkyboxTrans trans{proj_view_model};
  model_->BindPipeline(ctx, frame_size);
  ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                    sizeof(SkyboxTrans), &trans);
  model_->Draw(ctx);
}

}  // namespace lighter::scene
