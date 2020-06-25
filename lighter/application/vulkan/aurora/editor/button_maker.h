//
//  button_maker.h
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_MAKER_H
#define LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_MAKER_H

#include <memory>
#include <string>
#include <vector>

#include "lighter/application/vulkan/aurora/editor/button_util.h"
#include "lighter/common/file.h"
#include "lighter/renderer/vulkan/extension/text.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace make_button {

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct RenderInfo {
  glm::vec3 color;
  glm::vec2 center;
};

/* END: Consistent with vertex input attributes defined in shaders. */

// Configures how to render one button in all states.
struct ButtonInfo {
  std::string text;
  RenderInfo render_info[button::kNumStates];
  float base_y[button::kNumStates];
  float height[button::kNumStates];
};

} /* namespace make_button */

// This class is used to render multiple buttons onto a big texture, so that
// to render all buttons later, we only need to bind one texture and emit one
// render call.
class ButtonMaker {
 public:
  struct RenderInfo : public make_button::RenderInfo {
    // Returns vertex input attributes.
    static std::vector<renderer::vulkan::VertexBuffer::Attribute>
    GetAttributes() {
      return {{offsetof(RenderInfo, color), VK_FORMAT_R32G32B32_SFLOAT},
              {offsetof(RenderInfo, center), VK_FORMAT_R32G32_SFLOAT}};
    }
  };

  // Returns a texture that contains all buttons in all states. Layout:
  //
  //   |--------------------|
  //   |       ......       |
  //   |--------------------|
  //   | Button1 unselected |
  //   |--------------------|
  //   | Button1 selected   |
  //   |--------------------|
  //   | Button0 unselected |
  //   |--------------------|
  //   | Button0 selected   |
  //   |--------------------|
  //
  // Also note that buttons are opaque on this texture.
  static std::unique_ptr<renderer::vulkan::OffscreenImage> CreateButtonsImage(
      const renderer::vulkan::SharedBasicContext& context,
      renderer::vulkan::Text::Font font, int font_height,
      const glm::vec3& text_color, const common::Image& button_background,
      const button::VerticesInfo& vertices_info,
      absl::Span<const make_button::ButtonInfo> button_infos);
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_MAKER_H */
