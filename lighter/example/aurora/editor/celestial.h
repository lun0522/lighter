//
//  celestial.h
//
//  Created by Pujun Lun on 12/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_AURORA_EDITOR_CELESTIAL_H
#define LIGHTER_EXAMPLE_AURORA_EDITOR_CELESTIAL_H

#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/scene/model.h"
#include "lighter/scene/skybox.h"

namespace lighter::example::aurora {

// Renders the earth model and surrounding skybox inside a Frame Graph pass.
class Celestial {
 public:
  enum EarthTextureIndex {
    kEarthDayTextureIndex = 0,
    kEarthNightTextureIndex,
  };

  Celestial(const renderer::ir::Renderer& renderer, float viewport_aspect_ratio,
            int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Celestial(const Celestial&) = delete;
  Celestial& operator=(const Celestial&) = delete;

  // Renders earth and skybox into the active graphics pass using 'frame_slot'.
  void Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
            int frame_slot, EarthTextureIndex texture_index,
            const glm::mat4& earth_proj_view_model,
            const glm::mat4& skybox_proj_view_model) const;

 private:
  std::vector<std::unique_ptr<renderer::ir::Buffer>> earth_uniforms_;
  std::unique_ptr<scene::Model> earth_model_;
  std::unique_ptr<scene::Skybox> skybox_;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_EDITOR_CELESTIAL_H
