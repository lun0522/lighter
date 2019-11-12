//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/button.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/memory/memory.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

} /* namespace */

Button::Button(const SharedBasicContext& context,
               Text::Font font, int font_height,
               const glm::vec3& text_color, float text_alpha,
               const glm::vec2& button_size, float button_alpha,
               const std::vector<button::ButtonInfo>& button_infos) {
  const int num_buttons = button_infos.size();
  std::vector<std::string> texts;
  texts.reserve(num_buttons);
  for (const auto& info : button_infos) {
    texts.emplace_back(info.text);
  }
  DynamicText text_renderer{context, /*num_frames_in_flight=*/1,
                            /*viewport_aspect_ratio=*/1.0f,
                            texts, font, font_height};

  const SharedTexture button_image{
      context, common::file::GetResourcePath("texture/rect_rounded.jpg")};
  buttons_image_ = absl::make_unique<OffscreenImage>(
      context, /*channel=*/4, VkExtent2D{
          button_image->extent().width * button::ButtonInfo::kNumStates,
          static_cast<uint32_t>(button_image->extent().height * num_buttons),
      });
}

void Button::Draw(const VkCommandBuffer& command_buffer, int frame) const {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
