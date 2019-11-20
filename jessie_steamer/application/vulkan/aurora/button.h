//
//  button.h
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H

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

  struct Info {
    std::string text;
    glm::vec3 colors[kNumStates];
  };

  wrapper::vulkan::Text::Font font;
  int font_height;
  float base_y;
  float top_y;
  glm::vec3 text_color;
  float button_alphas[kNumStates];
  std::vector<Info> button_infos;
};

} /* namespace button */

class ButtonMaker {
 public:
  ButtonMaker(const wrapper::vulkan::SharedBasicContext& context,
              const button::ButtonInfo& button_info);

  // This class is neither copyable nor movable.
  ButtonMaker(const ButtonMaker&) = delete;
  ButtonMaker& operator=(const ButtonMaker&) = delete;

  wrapper::vulkan::OffscreenImagePtr buttons_image() const {
    return buttons_image_.get();
  }

 private:
  std::unique_ptr<wrapper::vulkan::OffscreenImage> buttons_image_;
};

class Button {
 public:
  Button(const wrapper::vulkan::SharedBasicContext& context,
         const button::ButtonInfo& button_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  void Draw(const VkCommandBuffer& command_buffer, int frame) const;

  wrapper::vulkan::OffscreenImagePtr backdoor_buttons_image() const {
    return button_maker_.buttons_image();
  }

 private:
  ButtonMaker button_maker_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H */
