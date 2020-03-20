//
//  button_maker.h
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_MAKER_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_MAKER_H

#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/application/vulkan/aurora/editor/button_util.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/text.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
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
    explicit RenderInfo(const make_button::RenderInfo& info)
        : make_button::RenderInfo{info} {}

    // Returns vertex input attributes.
    static std::vector<wrapper::vulkan::VertexBuffer::Attribute>
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
  static std::unique_ptr<wrapper::vulkan::OffscreenImage> CreateButtonsImage(
      const wrapper::vulkan::SharedBasicContext& context,
      wrapper::vulkan::Text::Font font, int font_height,
      const glm::vec3& text_color, const common::Image& button_background,
      const button::VerticesInfo& vertices_info,
      absl::Span<const make_button::ButtonInfo> button_infos);
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_MAKER_H */
