//
//  button.h
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_UI_BUTTON_H
#define LIGHTER_UI_BUTTON_H

#include <array>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "lighter/base/data.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/ui/text.h"

namespace lighter::ui {
namespace button {

enum State { kSelectedState = 0, kUnselectedState, kNumStates };

// Consistent with uniform blocks defined in shaders.
constexpr int kNumVerticesPerButton = 6;

struct VerticesInfo {
  ALIGN_VEC4 glm::vec4 pos_tex_coords[kNumVerticesPerButton];
};

}  // namespace button

namespace make_button {
struct RenderInfo;
}  // namespace make_button

namespace draw_button {

// Consistent with vertex input attributes defined in shaders.
struct RenderInfo {
  static std::vector<renderer::ir::VertexBufferView::Attribute>
  GetVertexAttributes();

  float alpha;
  glm::vec2 pos_center_ndc;
  glm::vec2 tex_coord_center;
};

}  // namespace draw_button

// Renders multiple instanced buttons in one draw call inside a Frame Graph
// graphics pass.
class ButtonRenderer {
 public:
  ButtonRenderer(const renderer::ir::Renderer& renderer, int num_buttons,
                 int num_frames_in_flight,
                 const button::VerticesInfo& vertices_info,
                 std::unique_ptr<renderer::ir::Image>&& buttons_image);

  // This class is neither copyable nor movable.
  ButtonRenderer(const ButtonRenderer&) = delete;
  ButtonRenderer& operator=(const ButtonRenderer&) = delete;

  void Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
            float viewport_aspect_ratio, int frame_slot,
            std::span<const draw_button::RenderInfo> buttons_to_render) const;

 private:
  const std::unique_ptr<renderer::ir::Image> buttons_image_;
  std::vector<std::unique_ptr<renderer::ir::Buffer>> per_instance_buffers_;
  std::unique_ptr<renderer::ir::Buffer> vertices_uniform_;
  renderer::ir::SamplerDescriptor sampler_{};
  renderer::ir::GraphicsPipelineDescriptor base_pipeline_descriptor_;
};

// Manages a row of UI buttons (atlas baking, rendering, and click hit-testing).
class Button {
 public:
  struct ButtonsInfo {
    struct Info {
      std::string text;
      std::array<glm::vec3, button::kNumStates> colors;
      glm::vec2 center;
    };

    Text::Font font;
    int font_height;
    float base_y;
    float top_y;
    glm::vec3 text_color;
    std::array<float, button::kNumStates> button_alphas;
    glm::vec2 button_size;
    std::span<const Info> button_infos;
  };

  enum class State { kHidden, kSelected, kUnselected };

  Button(renderer::ir::Renderer& renderer, float viewport_aspect_ratio,
         int num_frames_in_flight, const ButtonsInfo& buttons_info);

  // This class is neither copyable nor movable.
  Button(const Button&) = delete;
  Button& operator=(const Button&) = delete;

  void Draw(renderer::ir::PassContext& ctx, const glm::ivec2& frame_size,
            int frame_slot, std::span<const State> button_states);

  std::optional<int> GetClickedButtonIndex(
      const glm::vec2& click_ndc, int button_index_offset,
      std::span<const State> button_states) const;

 private:
  using DrawButtonRenderInfos =
      std::vector<std::array<draw_button::RenderInfo, button::kNumStates>>;

  struct TextPos {
    float base_y;
    float height;
  };

  std::vector<make_button::RenderInfo> CreateMakeButtonRenderInfos(
      const ButtonsInfo& buttons_info) const;
  button::VerticesInfo CreateMakeButtonVerticesInfo(
      int num_buttons, const glm::vec2& button_scale) const;
  std::vector<TextPos> CreateMakeButtonTextPos(
      const ButtonsInfo& buttons_info) const;
  DrawButtonRenderInfos ExtractDrawButtonRenderInfos(
      const ButtonsInfo& buttons_info) const;
  button::VerticesInfo CreateDrawButtonVerticesInfo(
      const ButtonsInfo& buttons_info, const glm::vec2& button_uv_scale) const;

  const float viewport_aspect_ratio_;
  const glm::vec2 button_half_size_ndc_;
  const DrawButtonRenderInfos all_buttons_;
  std::vector<draw_button::RenderInfo> buttons_to_render_;
  std::unique_ptr<ButtonRenderer> button_renderer_;
};

}  // namespace lighter::ui

#endif  // LIGHTER_UI_BUTTON_H
