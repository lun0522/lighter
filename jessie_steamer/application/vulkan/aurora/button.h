//
//  button.h
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H

#include <array>
#include <string>

#include "jessie_steamer/application/vulkan/util.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace button {

enum State { kSelected = 0, kUnselected, kNumStates };

constexpr int kNumVerticesPerRect = 4;

struct RenderInfo {
  glm::vec4 color_alpha[kNumStates];
  glm::vec2 position[kNumVerticesPerRect];
};

} /* namespace button */

template <int NumButtons>
class Button {
 public:
  Button(const std::array<std::string, NumButtons>& button_texts,
         const std::array<glm::vec4[button::kNumStates], NumButtons>&
             button_color_alphas,
         const std::array<glm::vec4[button::kNumStates], NumButtons>&
             text_color_alphas);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

 private:
  std::array<button::RenderInfo, NumButtons> button_render_infos_;
  std::array<button::RenderInfo, NumButtons> text_render_infos_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H */
