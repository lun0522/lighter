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
  Button(wrapper::vulkan::SharedBasicContext context,
         float viewport_aspect_ratio,
         const button::ButtonInfo& button_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  void Update(const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const wrapper::vulkan::RenderPass& render_pass,
              uint32_t subpass_index);

  // TODO
  void Draw(const VkCommandBuffer& command_buffer, int frame,
            int num_buttons) const;

  wrapper::vulkan::OffscreenImagePtr backdoor_buttons_image() const {
    return button_maker_.buttons_image();
  }

 private:
  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // buttons does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  ButtonMaker button_maker_;
  std::unique_ptr<wrapper::vulkan::PerInstanceBuffer> per_instance_buffer_;
  std::unique_ptr<wrapper::vulkan::PushConstant> push_constant_;
  std::unique_ptr<wrapper::vulkan::StaticDescriptor> descriptor_;
  wrapper::vulkan::PipelineBuilder pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_BUTTON_H */
