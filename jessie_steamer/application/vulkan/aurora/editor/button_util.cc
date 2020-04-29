//
//  button_util.cc
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/button_util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace button {

void SetVerticesPositions(const glm::vec2& size_ndc, VerticesInfo* info) {
  const auto set_pos = [info](int index, float x, float y) {
    info->pos_tex_coords[index].x = x;
    info->pos_tex_coords[index].y = y;
  };
  const glm::vec2 half_size_ndc = size_ndc / 2.0f;
  set_pos(0, -half_size_ndc.x, -half_size_ndc.y);
  set_pos(1,  half_size_ndc.x, -half_size_ndc.y);
  set_pos(2,  half_size_ndc.x,  half_size_ndc.y);
  set_pos(3, -half_size_ndc.x, -half_size_ndc.y);
  set_pos(4,  half_size_ndc.x,  half_size_ndc.y);
  set_pos(5, -half_size_ndc.x,  half_size_ndc.y);
}

void SetVerticesTexCoords(const glm::vec2& center_uv, const glm::vec2& size_uv,
                          VerticesInfo* info) {
  const auto set_tex_coord = [info, &center_uv](int index, float x, float y) {
    info->pos_tex_coords[index].z = center_uv.x + x;
    info->pos_tex_coords[index].w = center_uv.y + y;
  };
  const glm::vec2 half_size_uv = size_uv / 2.0f;
  set_tex_coord(0, -half_size_uv.x, -half_size_uv.y);
  set_tex_coord(1,  half_size_uv.x, -half_size_uv.y);
  set_tex_coord(2,  half_size_uv.x,  half_size_uv.y);
  set_tex_coord(3, -half_size_uv.x, -half_size_uv.y);
  set_tex_coord(4,  half_size_uv.x,  half_size_uv.y);
  set_tex_coord(5, -half_size_uv.x,  half_size_uv.y);
}

} /* namespace button */
} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
