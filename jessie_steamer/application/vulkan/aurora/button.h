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
#include "absl/types/optional.h"
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
    glm::vec2 center;
  };

  // 'base_y' and 'top_y' are in range [0.0, 1.0]. They control where do we
  // render text within each button.
  wrapper::vulkan::Text::Font font;
  int font_height;
  float base_y;
  float top_y;
  glm::vec3 text_color;
  float button_alphas[kNumStates];
  glm::vec2 button_size;
  std::vector<Info> button_infos;
};

} /* namespace button */

class ButtonMaker {
 public:
  using ButtonInfo = button::ButtonInfo;

  ButtonMaker(const wrapper::vulkan::SharedBasicContext& context,
              const ButtonInfo& button_info);

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
  using ButtonInfo = button::ButtonInfo;

  enum class State { kHidden, kSelected, kUnselected };

  Button(wrapper::vulkan::SharedBasicContext context,
         float viewport_aspect_ratio, const ButtonInfo& button_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  void Update(const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const wrapper::vulkan::RenderPass& render_pass,
              uint32_t subpass_index);

  void Draw(const VkCommandBuffer& command_buffer,
            const std::vector<State>& button_states);

  absl::optional<int> GetClickedButtonIndex(
      const glm::vec2& click_ndc,
      const std::vector<State>& button_states) const;

 private:
  /* BEGIN: Consistent with vertex input attributes defined in shaders. */

  struct ButtonRenderInfo {
    float alpha;
    glm::vec2 pos_center_ndc;
    glm::vec2 tex_coord_center;
  };

  /* END: Consistent with vertex input attributes defined in shaders. */

  using ButtonRenderInfos =
      std::vector<std::array<ButtonRenderInfo, ButtonInfo::kNumStates>>;

  ButtonRenderInfos ExtractRenderInfos(
      const ButtonInfo& button_info) const;

  std::unique_ptr<wrapper::vulkan::PushConstant> CreateButtonVerticesInfo(
      const wrapper::vulkan::SharedBasicContext& context,
      const ButtonInfo& button_info) const;

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // buttons does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  const glm::vec2 button_half_size_ndc_;

  const ButtonRenderInfos all_buttons_;

  ButtonMaker button_maker_;
  std::vector<ButtonRenderInfo> buttons_to_render_;
  std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
      per_instance_buffer_;
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
