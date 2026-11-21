//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/aurora/editor/button.h"

#include <algorithm>
#include <cstddef>
#include <format>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/example/util.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::example::aurora {
namespace {

using renderer::ir::AccessLocation;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::DataFormat;
using renderer::ir::FilterType;
using renderer::ir::GraphicsPipelineDescriptor;
using renderer::ir::Image;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::SamplerAddressMode;
using renderer::ir::VertexBufferView;
using renderer::ir::VertexInputRate;

constexpr uint32_t kVerticesInfoBindingPoint = 0;
constexpr uint32_t kImageBindingPoint = 1;
constexpr float kNdcDim = 1.0f - (-1.0f);
constexpr float kUvDim = 1.0f;
constexpr uint32_t kPerInstanceBufferBindingPoint = 0;

}  // namespace

namespace draw_button {

std::vector<VertexBufferView::Attribute> RenderInfo::GetVertexAttributes() {
  return {
      {/*location=*/0, DataFormat::kSFloat32, offsetof(RenderInfo, alpha)},
      {/*location=*/1, DataFormat::kSFloat32Vec2,
       offsetof(RenderInfo, pos_center_ndc)},
      {/*location=*/2, DataFormat::kSFloat32Vec2,
       offsetof(RenderInfo, tex_coord_center)},
  };
}

}  // namespace draw_button

ButtonRenderer::ButtonRenderer(const Renderer& renderer, int num_buttons,
                               int num_frames_in_flight,
                               const button::VerticesInfo& vertices_info,
                               std::unique_ptr<Image>&& buttons_image)
    : buttons_image_{std::move(buttons_image)} {
  const std::array instance_usages{
      BufferUsage::GetVertexBufferUsage(BufferUsage::UsageType::kVertexOnly)};
  const size_t max_instance_bytes =
      sizeof(draw_button::RenderInfo) * num_buttons * button::kNumStates;
  per_instance_buffers_.resize(num_frames_in_flight);
  for (auto& buffer : per_instance_buffers_) {
    buffer = renderer.CreateBuffer(Buffer::UpdateRate::kHigh,
                                   max_instance_bytes, instance_usages);
  }

  vertices_uniform_ = renderer.CreateBuffer(
      Buffer::UpdateRate::kLow, sizeof(button::VerticesInfo),
      std::array{
          BufferUsage::GetUniformBufferUsage(AccessLocation::kVertexShader)});
  vertices_uniform_->CopyToDevice(std::array{
      Buffer::CopyInfo{&vertices_info, sizeof(button::VerticesInfo), 0}});

  sampler_ = {
      .filter_type = FilterType::kLinear,
      .address_mode = SamplerAddressMode::kClampToEdge,
  };

  base_pipeline_descriptor_.SetName("DrawButton")
      .SetShader(renderer::ir::shader_stage::VERTEX,
                 GetShaderBinaryPath("aurora/draw_button.vert"))
      .SetShader(renderer::ir::shader_stage::FRAGMENT,
                 GetShaderBinaryPath("aurora/draw_button.frag"))
      .UseColorAttachment(/*location=*/0,
                          renderer::ir::pipeline::GetColorAlphaBlend())
      .AddVertexInput({
          VertexInputRate::kInstance,
          kPerInstanceBufferBindingPoint,
          sizeof(draw_button::RenderInfo),
          draw_button::RenderInfo::GetVertexAttributes(),
      });
}

void ButtonRenderer::Draw(
    PassContext& ctx, const glm::ivec2& frame_size, float viewport_aspect_ratio,
    int frame_slot,
    std::span<const draw_button::RenderInfo> buttons_to_render) const {
  if (buttons_to_render.empty()) {
    return;
  }
  const Buffer& instance_buffer = *per_instance_buffers_[frame_slot];
  instance_buffer.CopyToDevice(std::array{Buffer::CopyInfo{
      buttons_to_render.data(),
      common::util::GetTotalDataSize(buttons_to_render),
      /*offset=*/0,
  }});

  GraphicsPipelineDescriptor desc = base_pipeline_descriptor_;
  desc.SetViewport(
      renderer::ir::pipeline::GetViewport(frame_size, viewport_aspect_ratio),
      renderer::ir::pipeline::GetFullFrameScissor(frame_size), /*flip_y=*/true);

  ctx.BindGraphicsPipeline(desc);
  ctx.BindVertexBuffer(kPerInstanceBufferBindingPoint, instance_buffer);
  ctx.BindBuffer(kVerticesInfoBindingPoint, *vertices_uniform_);
  ctx.BindExternalImage(kImageBindingPoint, *buttons_image_, &sampler_);
  ctx.Draw(button::kNumVerticesPerButton,
           static_cast<uint32_t>(buttons_to_render.size()));
}

Button::Button(Renderer& renderer, float viewport_aspect_ratio,
               int num_frames_in_flight, const ButtonsInfo& buttons_info)
    : viewport_aspect_ratio_{viewport_aspect_ratio},
      button_half_size_ndc_{buttons_info.button_size * kNdcDim / 2.0f},
      all_buttons_{ExtractDrawButtonRenderInfos(buttons_info)} {
  const int num_buttons = static_cast<int>(buttons_info.button_infos.size());
  buttons_to_render_.reserve(static_cast<size_t>(num_buttons));

  // Background image can be any single channel image.
  const auto& button_size = buttons_info.button_size;
  constexpr int kBackgroundImageWidth = 500;
  const int background_image_height =
      static_cast<int>(kBackgroundImageWidth * button_size.y / button_size.x);
  const std::vector<char> background_image_pixels(
      static_cast<size_t>(kBackgroundImageWidth) * background_image_height,
      static_cast<char>(255));
  const auto background_image = common::Image::LoadSingleImageFromMemory(
      /*dimension=*/
      {
          kBackgroundImageWidth,
          background_image_height,
          common::image::kBwImageChannel,
      },
      background_image_pixels.data(),
      /*flip_y=*/false);
  const glm::vec2 background_image_size{background_image.extent()};

  // Leave spacing between baked buttons so linear filtering avoids bleeding.
  constexpr float kButtonDimensionToIntervalRatio = 100.0f;
  const glm::vec2 interval_candidates =
      background_image_size / kButtonDimensionToIntervalRatio;
  const float button_interval =
      std::max(interval_candidates.x, interval_candidates.y);
  const glm::vec2 button_uv_scale =
      background_image_size / (background_image_size + button_interval);

  const auto render_infos = CreateMakeButtonRenderInfos(buttons_info);
  const auto text_pos = CreateMakeButtonTextPos(buttons_info);
  std::vector<make_button::ButtonInfo> button_infos;
  button_infos.reserve(static_cast<size_t>(num_buttons));
  for (int button = 0; button < num_buttons; ++button) {
    const int index_base = button * button::kNumStates;
    button_infos.push_back(make_button::ButtonInfo{
        buttons_info.button_infos[button].text,
        {render_infos[index_base], render_infos[index_base + 1]},
        {text_pos[index_base].base_y, text_pos[index_base + 1].base_y},
        {text_pos[index_base].height, text_pos[index_base + 1].height},
    });
  }
  auto buttons_image = ButtonMaker::CreateButtonsImage(
      renderer, buttons_info.font, buttons_info.font_height,
      buttons_info.text_color, background_image,
      CreateMakeButtonVerticesInfo(num_buttons, button_uv_scale), button_infos);

  button_renderer_ = std::make_unique<ButtonRenderer>(
      renderer, num_buttons, num_frames_in_flight,
      CreateDrawButtonVerticesInfo(buttons_info, button_uv_scale),
      std::move(buttons_image));
}

std::vector<make_button::RenderInfo> Button::CreateMakeButtonRenderInfos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = static_cast<int>(buttons_info.button_infos.size());
  const float button_height_ndc =
      kNdcDim / static_cast<float>(num_buttons * button::kNumStates);
  float offset_y_ndc = -1.0f + button_height_ndc / 2.0f;
  std::vector<make_button::RenderInfo> render_infos;
  render_infos.reserve(static_cast<size_t>(num_buttons) * button::kNumStates);
  for (const auto& info : buttons_info.button_infos) {
    for (int state = 0; state < button::kNumStates; ++state) {
      render_infos.push_back(make_button::RenderInfo{
          info.colors[state],
          /*center=*/glm::vec2{0.0f, offset_y_ndc},
      });
      offset_y_ndc += button_height_ndc;
    }
  }
  return render_infos;
}

button::VerticesInfo Button::CreateMakeButtonVerticesInfo(
    int num_buttons, const glm::vec2& button_scale) const {
  const float button_height_ndc =
      kNdcDim / static_cast<float>(num_buttons * button::kNumStates);
  const glm::vec2 button_size_ndc =
      glm::vec2{kNdcDim, button_height_ndc} * button_scale;

  button::VerticesInfo vertices_info{};
  SetVerticesPositions(button_size_ndc, &vertices_info);
  SetVerticesTexCoords(/*center_uv=*/glm::vec2{kUvDim} / 2.0f,
                       /*size_uv=*/glm::vec2{kUvDim}, &vertices_info);
  return vertices_info;
}

std::vector<Button::TextPos> Button::CreateMakeButtonTextPos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = static_cast<int>(buttons_info.button_infos.size());
  const float button_height =
      kUvDim / static_cast<float>(num_buttons * button::kNumStates);
  const float text_height =
      (buttons_info.top_y - buttons_info.base_y) * button_height;

  float offset_y = 0.0f;
  std::vector<TextPos> text_pos;
  text_pos.reserve(static_cast<size_t>(num_buttons) * button::kNumStates);
  for (int button = 0; button < num_buttons; ++button) {
    for (int state = 0; state < button::kNumStates; ++state) {
      const float base_y = offset_y + buttons_info.base_y * button_height;
      text_pos.push_back(TextPos{base_y, text_height});
      offset_y += button_height;
    }
  }
  return text_pos;
}

Button::DrawButtonRenderInfos Button::ExtractDrawButtonRenderInfos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = static_cast<int>(buttons_info.button_infos.size());
  const float button_tex_height =
      kUvDim / static_cast<float>(num_buttons * button::kNumStates);
  constexpr float kTexCenterOffsetX = kUvDim / 2.0f;
  float tex_center_offset_y = button_tex_height / 2.0f;

  DrawButtonRenderInfos render_infos;
  render_infos.reserve(static_cast<size_t>(num_buttons));
  for (const auto& info : buttons_info.button_infos) {
    const glm::vec2 pos_center_ndc = info.center * 2.0f - 1.0f;
    render_infos.push_back(
        std::array<draw_button::RenderInfo, button::kNumStates>{
            draw_button::RenderInfo{
                buttons_info.button_alphas[button::kSelectedState],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX, tex_center_offset_y}},
            draw_button::RenderInfo{
                buttons_info.button_alphas[button::kUnselectedState],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX,
                          tex_center_offset_y + button_tex_height}},
        });
    tex_center_offset_y += 2 * button_tex_height;
  }
  return render_infos;
}

button::VerticesInfo Button::CreateDrawButtonVerticesInfo(
    const ButtonsInfo& buttons_info, const glm::vec2& button_uv_scale) const {
  const glm::vec2 button_size_ndc = buttons_info.button_size * kNdcDim;
  const int num_buttons = static_cast<int>(buttons_info.button_infos.size());
  const float button_tex_height =
      kUvDim / static_cast<float>(num_buttons * button::kNumStates);
  const glm::vec2 size_uv =
      glm::vec2{kUvDim, button_tex_height} * button_uv_scale;

  button::VerticesInfo vertices_info{};
  SetVerticesPositions(button_size_ndc, &vertices_info);
  SetVerticesTexCoords(/*center_uv=*/glm::vec2{0.0f}, size_uv, &vertices_info);
  return vertices_info;
}

void Button::Draw(PassContext& ctx, const glm::ivec2& frame_size,
                  int frame_slot, std::span<const State> button_states) {
  const size_t num_buttons = all_buttons_.size();
  ASSERT_TRUE(button_states.size() == num_buttons,
              std::format("Length of button states ({}) must match with "
                          "number of buttons ({})",
                          button_states.size(), num_buttons));

  buttons_to_render_.clear();
  for (size_t button = 0; button < num_buttons; ++button) {
    switch (button_states[button]) {
      case State::kHidden:
        break;
      case State::kSelected:
        buttons_to_render_.push_back(
            all_buttons_[button][button::kSelectedState]);
        break;
      case State::kUnselected:
        buttons_to_render_.push_back(
            all_buttons_[button][button::kUnselectedState]);
        break;
    }
  }
  button_renderer_->Draw(ctx, frame_size, viewport_aspect_ratio_, frame_slot,
                         buttons_to_render_);
}

std::optional<int> Button::GetClickedButtonIndex(
    const glm::vec2& click_ndc, int button_index_offset,
    std::span<const State> button_states) const {
  const size_t num_buttons = all_buttons_.size();
  ASSERT_TRUE(button_states.size() == num_buttons,
              std::format("Length of button states ({}) must match with "
                          "number of buttons ({})",
                          button_states.size(), num_buttons));

  for (size_t i = 0; i < num_buttons; ++i) {
    if (button_states[i] == State::kHidden) {
      continue;
    }
    const glm::vec2 distance =
        glm::abs(click_ndc - all_buttons_[i][0].pos_center_ndc);
    if (distance.x <= button_half_size_ndc_.x &&
        distance.y <= button_half_size_ndc_.y) {
      return button_index_offset + static_cast<int>(i);
    }
  }
  return std::nullopt;
}

}  // namespace lighter::example::aurora
