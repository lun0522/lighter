//
//  button.h
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/text.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace button {

struct ButtonInfo {
  enum State { kSelected = 0, kUnselected, kNumStates };

  std::string text;
  glm::vec3 colors[kNumStates];
  glm::vec2 center;
};

} /* namespace button */

class Button {
 public:
  Button(const wrapper::vulkan::SharedBasicContext& context,
         wrapper::vulkan::Text::Font font, int font_height,
         const glm::vec3& text_color, const glm::vec2& button_size,
         const std::array<float, button::ButtonInfo::kNumStates> button_alpha,
         const std::vector<button::ButtonInfo>& button_infos);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  void Draw(const VkCommandBuffer& command_buffer, int frame) const;

  wrapper::vulkan::OffscreenImagePtr backdoor_buttons_image() const {
    return buttons_image_.get();
  }

 private:
  std::unique_ptr<wrapper::vulkan::OffscreenImage> buttons_image_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H */
