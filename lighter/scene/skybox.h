//
//  skybox.h
//
//  Created by Pujun Lun on 11/20/26.
//  Copyright © 2026 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SCENE_SKYBOX_H
#define LIGHTER_SCENE_SKYBOX_H

#include <glm/glm.hpp>
#include <memory>

#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/scene/model.h"

namespace lighter::scene {

// Renders a cubemap skybox using a unit cube mesh and shared/skybox shaders.
class Skybox {
 public:
  Skybox(const renderer::ir::Renderer& renderer, float viewport_aspect_ratio,
         const ModelBuilder::CubemapPath& cubemap_path);

  // This class is neither copyable nor movable.
  Skybox(const Skybox&) = delete;
  Skybox& operator=(const Skybox&) = delete;

  // Binds the skybox pipeline, pushes 'proj_view_model', and issues the draw
  // inside an active graphics pass.
  void Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
            const glm::mat4& proj_view_model) const;

 private:
  std::unique_ptr<Model> model_;
};

}  // namespace lighter::scene

#endif  // LIGHTER_SCENE_SKYBOX_H
