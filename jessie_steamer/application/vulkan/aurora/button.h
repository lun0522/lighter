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

#include "jessie_steamer/common/util.h"
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

// Contains information for rendering multiple buttons onto a big texture.
struct ButtonInfo {
  enum State { kSelected = 0, kUnselected, kNumStates };

  // Contains information for rendering a single button.
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

// This class is used to render multiple buttons onto a big texture, so that
// to render all buttons later, we only need to bind one texture and emit one
// render call. The user should simply discard the instance of this class after
// calling CreateButtonsImage().
class ButtonMaker {
 public:
  using ButtonInfo = button::ButtonInfo;

  // The caller is responsible for keeping the existence of 'button_info' util
  // finish using this button maker.
  ButtonMaker(wrapper::vulkan::SharedBasicContext context,
              const ButtonInfo* button_info)
      : context_{std::move(context)}, button_info_{*button_info},
        num_buttons_{static_cast<int>(button_info_.button_infos.size())} {
    ASSERT_NON_NULL(button_info, "Button info must not be nullptr");
  }

  // This class is neither copyable nor movable.
  ButtonMaker(const ButtonMaker&) = delete;
  ButtonMaker& operator=(const ButtonMaker&) = delete;

  // Returns a texture that contains all buttons in all states. Layout:
  //
  //   |--------------------|
  //   | Button0 selected   |
  //   |--------------------|
  //   | Button0 unselected |
  //   |--------------------|
  //   | Button1 selected   |
  //   |--------------------|
  //   | Button1 unselected |
  //   |--------------------|
  //   |       ......       |
  //   |--------------------|
  //
  // This layout has been flipped in Y-axis for readability.
  // Also note that buttons are not transparent on this texture.
  std::unique_ptr<wrapper::vulkan::OffscreenImage> CreateButtonsImage() const;

 private:
  enum SubpassIndex {
    kBackgroundSubpassIndex = 0,
    kTextSubpassIndex,
    kNumSubpasses,
  };

  /* BEGIN: Consistent with vertex input attributes defined in shaders. */

  struct ButtonRenderInfo {
    static std::vector<wrapper::vulkan::VertexBuffer::Attribute>
    GetAttributes() {
      return {
          {offsetof(ButtonRenderInfo, color), VK_FORMAT_R32G32B32_SFLOAT},
          {offsetof(ButtonRenderInfo, center), VK_FORMAT_R32G32_SFLOAT},
      };
    }

    glm::vec3 color;
    glm::vec2 center;
  };

  /* END: Consistent with vertex input attributes defined in shaders. */

  // Returns a PerInstanceBuffer that stores ButtonRenderInfo for all buttons in
  // all states.
  std::unique_ptr<wrapper::vulkan::PerInstanceBuffer>
  CreatePerInstanceData() const;

  // Returns a PushConstant that stores the pos and tex_coord of each vertex.
  std::unique_ptr<wrapper::vulkan::PushConstant> CreateButtonVerticesData(
      const glm::vec2& background_image_size) const;

  // Returns a renderer for the texts on buttons.
  std::unique_ptr<wrapper::vulkan::Text> CreateTextRenderer(
      const wrapper::vulkan::Image& buttons_image,
      const wrapper::vulkan::RenderPass& render_pass) const;

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  // Buttons rendering information.
  const ButtonInfo& button_info_;

  // Number of buttons. Note that since each button has two states, there will
  // be 'num_buttons_' * 2 buttons on the output of CreateButtonsImage().
  const int num_buttons_;
};

// This class is used to render multiple buttons with one render call.
// These buttons will share:
//   - Text font, height, location within each button, and color.
//   - Transparency in each state (i.e. selected and unselected state).
//   - Size of the button.
// They don't share:
//   - Text on the button.
//   - Color of the button (we can have different colors for different buttons
//     in different states).
//   - Center of the button on the frame.
// Update() must have been called before calling Draw() for the first time, and
// whenever the render pass is changed.
class Button {
 public:
  using ButtonInfo = button::ButtonInfo;

  enum class State { kHidden, kSelected, kUnselected };

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  Button(wrapper::vulkan::SharedBasicContext context,
         float viewport_aspect_ratio, const ButtonInfo& button_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  void Update(const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
              const wrapper::vulkan::RenderPass& render_pass,
              uint32_t subpass_index);

  // Renders all buttons. Buttons in 'State::kHidden' will not be rendered.
  // Others will be rendered with color and alpha selected according to states.
  // The size of 'button_states' must be equal to the size of
  // 'button_info.button_infos' passed to the constructor.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            const std::vector<State>& button_states);

  // If any button is clicked, returns the index of it. Otherwise, returns
  // absl::nullopt. If the current state of a button is 'State::kHidden', it
  // will be ignored in this click detection.
  absl::optional<int> GetClickedButtonIndex(
      const glm::vec2& click_ndc,
      const std::vector<State>& button_states) const;

 private:
  /* BEGIN: Consistent with vertex input attributes defined in shaders. */

  struct ButtonRenderInfo {
    static std::vector<wrapper::vulkan::VertexBuffer::Attribute>
    GetAttributes() {
      return {
          {offsetof(ButtonRenderInfo, alpha), VK_FORMAT_R32_SFLOAT},
          {offsetof(ButtonRenderInfo, pos_center_ndc), VK_FORMAT_R32G32_SFLOAT},
          {offsetof(ButtonRenderInfo, tex_coord_center),
           VK_FORMAT_R32G32_SFLOAT},
      };
    }

    float alpha;
    glm::vec2 pos_center_ndc;
    glm::vec2 tex_coord_center;
  };

  /* END: Consistent with vertex input attributes defined in shaders. */

  // The first dimension is different buttons, and the second dimension is
  // different states of one button.
  using ButtonRenderInfos =
      std::vector<std::array<ButtonRenderInfo, ButtonInfo::kNumStates>>;

  // Extract ButtonRenderInfos from 'button_info'.
  ButtonRenderInfos ExtractRenderInfos(
      const ButtonInfo& button_info) const;

  // Returns a PushConstant that stores the pos and tex_coord of each vertex.
  std::unique_ptr<wrapper::vulkan::PushConstant> CreateButtonVerticesData(
      const wrapper::vulkan::SharedBasicContext& context,
      const ButtonInfo& button_info) const;

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // buttons does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Size of each button on the frame in the normalized device coordinate.
  const glm::vec2 button_half_size_ndc_;

  // Rendering information for all buttons in all states.
  const ButtonRenderInfos all_buttons_;

  // Texture that contains all buttons in all states.
  const std::unique_ptr<wrapper::vulkan::OffscreenImage> buttons_image_;

  // Contains rendering information for buttons that will be rendered.
  std::vector<ButtonRenderInfo> buttons_to_render_;

  // Objects used for rendering.
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
