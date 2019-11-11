//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/button.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using std::array;

} /* namespace */

template <int NumButtons>
Button<NumButtons>::Button(
    const array<std::string, NumButtons>& button_texts,
    const array<glm::vec4[button::kNumStates], NumButtons>& button_color_alphas,
    const array<glm::vec4[button::kNumStates], NumButtons>& text_color_alphas) {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
