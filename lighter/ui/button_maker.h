//
//  button_maker.h
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_UI_BUTTON_MAKER_H
#define LIGHTER_UI_BUTTON_MAKER_H

#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/ui/button.h"
#include "lighter/ui/text.h"

namespace lighter::ui {
namespace make_button {

// Consistent with vertex input attributes defined in shaders.
struct RenderInfo {
  static std::vector<renderer::ir::VertexBufferView::Attribute>
  GetVertexAttributes();

  glm::vec3 color;
  glm::vec2 center;
};

// Configures how to render one button in all states.
struct ButtonInfo {
  std::string text;
  RenderInfo render_info[button::kNumStates];
  float base_y[button::kNumStates];
  float height[button::kNumStates];
};

}  // namespace make_button

// Bakes all buttons in all states onto a single offscreen texture atlas.
class ButtonMaker {
 public:
  static std::unique_ptr<renderer::ir::Image> CreateButtonsImage(
      renderer::ir::Renderer& renderer, Text::Font font, int font_height,
      const glm::vec3& text_color, const base::Image& button_background,
      const button::VerticesInfo& vertices_info,
      std::span<const make_button::ButtonInfo> button_infos);
};

}  // namespace lighter::ui

#endif  // LIGHTER_UI_BUTTON_MAKER_H
