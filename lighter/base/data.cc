//
//  data.cc
//
//  Created by Pujun Lun on 10/2/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/base/data.h"

namespace lighter::base {

std::array<Vertex2DPosOnly, 6> Vertex2DPosOnly::GetFullScreenSquadVertices() {
  return {
      Vertex2DPosOnly{.pos = {-1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = {1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = {1.0f, 1.0f}},
      Vertex2DPosOnly{.pos = {-1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = {1.0f, 1.0f}},
      Vertex2DPosOnly{.pos = {-1.0f, 1.0f}},
  };
}

std::array<Vertex2D, 6> Vertex2D::GetFullScreenSquadVertices(bool flip_y) {
  if (flip_y) {
    return {
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 1.0f}},
        Vertex2D{.pos = {1.0f, -1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {1.0f, 1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 1.0f}},
        Vertex2D{.pos = {1.0f, 1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {-1.0f, 1.0f}, .tex_coord = {0.0f, 0.0f}},
    };
  } else {
    return {
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 0.0f}},
        Vertex2D{.pos = {1.0f, -1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {1.0f, 1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 0.0f}},
        Vertex2D{.pos = {1.0f, 1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {-1.0f, 1.0f}, .tex_coord = {0.0f, 1.0f}},
    };
  }
}

}  // namespace lighter::base
