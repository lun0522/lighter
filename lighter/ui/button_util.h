//
//  button_util.h
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_UI_BUTTON_UTIL_H
#define LIGHTER_UI_BUTTON_UTIL_H

#include <glm/glm.hpp>

#include "lighter/base/data.h"

namespace lighter::ui::button {

enum State { kSelectedState = 0, kUnselectedState, kNumStates };

// Consistent with uniform blocks defined in shaders.
constexpr int kNumVerticesPerButton = 6;

struct VerticesInfo {
  ALIGN_VEC4 glm::vec4 pos_tex_coords[kNumVerticesPerButton];
};

// Sets the position part of VerticesInfo.
void SetVerticesPositions(const glm::vec2& size_ndc, VerticesInfo* info);

// Sets the texture coordinate part of VerticesInfo.
void SetVerticesTexCoords(const glm::vec2& center_uv, const glm::vec2& size_uv,
                          VerticesInfo* info);

}  // namespace lighter::ui::button

#endif  // LIGHTER_UI_BUTTON_UTIL_H
