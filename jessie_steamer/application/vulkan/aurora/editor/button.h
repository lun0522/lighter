//
//  button.h
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/application/vulkan/aurora/editor/state.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
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

/* BEGIN: Consistent with uniform blocks defined in shaders. */

constexpr int kNumVerticesPerButton = 6;

struct VerticesInfo {
  ALIGN_VEC4 glm::vec4 pos_tex_coords[kNumVerticesPerButton];
};

/* END: Consistent with uniform blocks defined in shaders. */

} /* namespace button */

namespace make_button {

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct RenderInfo {
  glm::vec3 color;
  glm::vec2 center;
};

/* END: Consistent with vertex input attributes defined in shaders. */

struct TextPos {
  float base_y;
  float height;
};

} /* namespace make_button */

namespace draw_button {

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

struct RenderInfo {
  float alpha;
  glm::vec2 pos_center_ndc;
  glm::vec2 tex_coord_center;
};

/* END: Consistent with vertex input attributes defined in shaders. */

} /* namespace draw_button */

// This class is used to render multiple buttons onto a big texture, so that
// to render all buttons later, we only need to bind one texture and emit one
// render call. The user should simply discard the instance of this class after
// calling CreateButtonsImage().
class ButtonMaker {
 public:
  ButtonMaker(wrapper::vulkan::SharedBasicContext context,
              wrapper::vulkan::Text::Font font, int font_height,
              std::vector<std::string>&& texts);

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
  void CreateButtonsImage(
      const button::VerticesInfo& vertices_info, const glm::vec3& text_color,
      const std::vector<make_button::RenderInfo>& render_infos,
      const std::vector<make_button::TextPos>& text_pos);

  std::unique_ptr<wrapper::vulkan::OffscreenImage> GetButtonsImage() {
    return std::move(buttons_image_);
  }

  // Accessors.
  const VkExtent2D& background_image_size() const {
    return (*background_image_)->extent();
  }
  float max_bearing_y() const { return text_renderer_->GetMaxBearingY(); }

 private:
  struct RenderInfo : public make_button::RenderInfo {
    static std::vector<wrapper::vulkan::VertexBuffer::Attribute>
    GetAttributes() {
      return {
          {offsetof(RenderInfo, color), VK_FORMAT_R32G32B32_SFLOAT},
          {offsetof(RenderInfo, center), VK_FORMAT_R32G32_SFLOAT},
      };
    }
  };

  // Returns a descriptor with an image bound to it.
  std::unique_ptr<wrapper::vulkan::StaticDescriptor> CreateDescriptor(
      const VkDescriptorImageInfo& image_info) const;

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  const std::vector<std::string> texts_;
  std::unique_ptr<wrapper::vulkan::SharedTexture> background_image_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> buttons_image_;
  std::unique_ptr<wrapper::vulkan::DynamicText> text_renderer_;
};

class ButtonRenderer {
 public:
  ButtonRenderer(
      const wrapper::vulkan::SharedBasicContext& context,
      int num_buttons, const button::VerticesInfo& vertices_info,
      std::unique_ptr<wrapper::vulkan::OffscreenImage>&& buttons_image);

  // This class is neither copyable nor movable.
  ButtonRenderer(const ButtonRenderer&) = delete;
  ButtonRenderer& operator=(const ButtonRenderer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(
      VkSampleCountFlagBits sample_count,
      const wrapper::vulkan::RenderPass& render_pass, uint32_t subpass_index,
      const wrapper::vulkan::PipelineBuilder::ViewportInfo& viewport);

  void Draw(const VkCommandBuffer& command_buffer,
            const std::vector<draw_button::RenderInfo>& buttons_to_render);

 private:
  struct RenderInfo : public draw_button::RenderInfo {
    static std::vector<wrapper::vulkan::VertexBuffer::Attribute>
    GetAttributes() {
      return {
          {offsetof(RenderInfo, alpha), VK_FORMAT_R32_SFLOAT},
          {offsetof(RenderInfo, pos_center_ndc), VK_FORMAT_R32G32_SFLOAT},
          {offsetof(RenderInfo, tex_coord_center), VK_FORMAT_R32G32_SFLOAT},
      };
    }
  };

  // Creates a descriptor for 'vertices_uniform_' and 'buttons_image_'.
  std::unique_ptr<wrapper::vulkan::StaticDescriptor> CreateDescriptor(
      const wrapper::vulkan::SharedBasicContext& context) const;

  // Texture that contains all buttons in all states.
  const std::unique_ptr<wrapper::vulkan::OffscreenImage> buttons_image_;

  // Objects used for rendering.
  std::unique_ptr<wrapper::vulkan::DynamicPerInstanceBuffer>
      per_instance_buffer_;
  std::unique_ptr<wrapper::vulkan::UniformBuffer> vertices_uniform_;
  std::unique_ptr<wrapper::vulkan::StaticDescriptor> descriptor_;
  wrapper::vulkan::PipelineBuilder pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
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
  // Contains information for rendering multiple buttons onto a big texture.
  struct ButtonsInfo {
    // Contains information for rendering a single button.
    struct Info {
      std::string text;
      std::array<glm::vec3, state::kNumStates> colors;
      glm::vec2 center;
    };

    // 'base_y' and 'top_y' are in range [0.0, 1.0]. They control where do we
    // render text within each button.
    wrapper::vulkan::Text::Font font;
    int font_height;
    float base_y;
    float top_y;
    glm::vec3 text_color;
    std::array<float, state::kNumStates> button_alphas;
    glm::vec2 button_size;
    std::vector<Info> button_infos;
  };

  // Possible states of each button.
  enum class State { kHidden, kSelected, kUnselected };

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  Button(const wrapper::vulkan::SharedBasicContext& context,
         float viewport_aspect_ratio, const ButtonsInfo& buttons_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         VkSampleCountFlagBits sample_count,
                         const wrapper::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Renders all buttons. Buttons in 'State::kHidden' will not be rendered.
  // Others will be rendered with color and alpha selected according to states.
  // The size of 'button_states' must be equal to the size of
  // 'buttons_info.button_infos' passed to the constructor.
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
  // The first dimension is different buttons, and the second dimension is
  // different states of one button.
  using DrawButtonRenderInfos =
      std::vector<std::array<draw_button::RenderInfo, state::kNumStates>>;

  // Returns a list of make_button::RenderInfo for all buttons in all states.
  std::vector<make_button::RenderInfo> CreateMakeButtonRenderInfos(
      const ButtonsInfo& buttons_info) const;

  // Returns a button::VerticesInfo that stores the pos and tex_coord of each
  // vertex.
  button::VerticesInfo CreateMakeButtonVerticesInfo(
      int num_buttons, const glm::vec2& background_image_size) const;

  std::vector<make_button::TextPos> CreateMakeButtonTextPos(
      float max_bearing_y, const ButtonsInfo& buttons_info) const;

  // Extract draw_button::RenderInfo from 'buttons_info'.
  DrawButtonRenderInfos ExtractDrawButtonRenderInfos(
      const ButtonsInfo& buttons_info) const;

  // Returns a button::VerticesInfo that stores the position and texture
  // coordinate of each vertex.
  button::VerticesInfo CreateDrawButtonVerticesInfo(
      const ButtonsInfo& buttons_info) const;

  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // buttons does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Size of each button on the frame in the normalized device coordinate.
  const glm::vec2 button_half_size_ndc_;

  // Rendering information for all buttons in all states.
  const DrawButtonRenderInfos all_buttons_;

  // Contains rendering information for buttons that will be rendered.
  std::vector<draw_button::RenderInfo> buttons_to_render_;

  std::unique_ptr<ButtonRenderer> button_renderer_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_BUTTON_H */
