//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/button.h"

#include <algorithm>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using std::vector;

enum UniformBindingPoint {
  kVerticesInfoBindingPoint = 0,
  kImageBindingPoint,
};

constexpr float kNdcDim = 1.0f - (-1.0f);
constexpr float kUvDim = 1.0f;
constexpr uint32_t kPerInstanceBufferBindingPoint = 0;

} /* namespace */

ButtonRenderer::ButtonRenderer(
    const SharedBasicContext& context,
    int num_buttons, const button::VerticesInfo& vertices_info,
    std::unique_ptr<OffscreenImage>&& buttons_image)
    : buttons_image_{std::move(buttons_image)}, pipeline_builder_{context} {
  per_instance_buffer_ = absl::make_unique<DynamicPerInstanceBuffer>(
      context, sizeof(RenderInfo),
      /*max_num_instances=*/num_buttons * button::kNumStates,
      RenderInfo::GetAttributes());

  vertices_uniform_ = absl::make_unique<UniformBuffer>(
      context, sizeof(button::VerticesInfo), /*num_frames_in_flight=*/1);
  *vertices_uniform_->HostData<button::VerticesInfo>(/*chunk_index=*/0) =
      vertices_info;
  vertices_uniform_->Flush(/*chunk_index=*/0);

  descriptor_ = CreateDescriptor(context);

  pipeline_builder_
      .SetName("draw button")
      .AddVertexInput(
          kPerInstanceBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<RenderInfo>(),
          per_instance_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_->layout()}, /*push_constant_ranges=*/{})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("draw_button.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("draw_button.frag"));
}

std::unique_ptr<StaticDescriptor> ButtonRenderer::CreateDescriptor(
    const SharedBasicContext& context) const {
  auto descriptor = absl::make_unique<StaticDescriptor>(
      context, /*infos=*/vector<Descriptor::Info>{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              VK_SHADER_STAGE_VERTEX_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kVerticesInfoBindingPoint,
                      /*array_length=*/1,
                  },
              },
          },
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }});
  descriptor->UpdateBufferInfos(
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      /*buffer_info_map=*/{
          {kVerticesInfoBindingPoint,
           {vertices_uniform_->GetDescriptorInfo(/*chunk_index=*/0)},
      }});
  descriptor->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*image_info_map=*/{
          {kImageBindingPoint, {buttons_image_->GetDescriptorInfo()}},
      });
  return descriptor;
}

void ButtonRenderer::UpdateFramebuffer(
    VkSampleCountFlagBits sample_count,
    const RenderPass& render_pass, uint32_t subpass_index,
    const PipelineBuilder::ViewportInfo& viewport) {
  pipeline_ = pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(PipelineBuilder::ViewportInfo{viewport})
      .SetRenderPass(*render_pass, subpass_index)
      .SetColorBlend(
          vector<VkPipelineColorBlendAttachmentState>(
              render_pass.num_color_attachments(subpass_index),
              pipeline::GetColorBlendState(/*enable_blend=*/true)))
      .Build();
}

void ButtonRenderer::Draw(
    const VkCommandBuffer& command_buffer,
    absl::Span<const draw_button::RenderInfo> buttons_to_render) {
  ASSERT_NON_NULL(pipeline_, "UpdateFramebuffer() must have been called");
  per_instance_buffer_->CopyHostData(buttons_to_render);
  pipeline_->Bind(command_buffer);
  per_instance_buffer_->Bind(
      command_buffer, kPerInstanceBufferBindingPoint, /*offset=*/0);
  descriptor_->Bind(command_buffer, pipeline_->layout());
  VertexBuffer::DrawWithoutBuffer(command_buffer, button::kNumVerticesPerButton,
                                  buttons_to_render.size());
}

Button::Button(const SharedBasicContext& context,
               float viewport_aspect_ratio,
               const ButtonsInfo& buttons_info)
    : viewport_aspect_ratio_{viewport_aspect_ratio},
      button_half_size_ndc_{buttons_info.button_size * kNdcDim / 2.0f},
      all_buttons_{ExtractDrawButtonRenderInfos(buttons_info)} {
  const int num_buttons = buttons_info.button_infos.size();
  buttons_to_render_.reserve(num_buttons);

  // Background image can be any single channel image.
  const auto& button_size = buttons_info.button_size;
  constexpr int kBackgroundImageWidth = 500;
  const auto background_image_height =
      static_cast<int>(kBackgroundImageWidth * button_size.y / button_size.x);
  const vector<char> background_image_pixels(
      kBackgroundImageWidth * background_image_height, static_cast<char>(255));
  const common::Image background_image{
      kBackgroundImageWidth, background_image_height, /*channel=*/1,
      background_image_pixels.data(), /*flip_y=*/false};
  const glm::vec2 background_image_size{background_image.width,
                                        background_image.height};

  const auto render_infos = CreateMakeButtonRenderInfos(buttons_info);
  const auto text_pos = CreateMakeButtonTextPos(buttons_info);
  vector<make_button::ButtonInfo> button_infos;
  button_infos.reserve(num_buttons);
  for (int button = 0; button < num_buttons; ++button) {
    const int index_base = button * button::kNumStates;
    button_infos.emplace_back(make_button::ButtonInfo{
        buttons_info.button_infos[button].text,
        {render_infos[index_base], render_infos[index_base + 1]},
        {text_pos[index_base].base_y, text_pos[index_base + 1].base_y},
        {text_pos[index_base].height, text_pos[index_base + 1].height},
    });
  }
  auto buttons_image = ButtonMaker::CreateButtonsImage(
      context, buttons_info.font, buttons_info.font_height,
      buttons_info.text_color, background_image,
      CreateMakeButtonVerticesInfo(num_buttons, background_image_size),
      button_infos);

  button_renderer_ = absl::make_unique<ButtonRenderer>(
      context, num_buttons, CreateDrawButtonVerticesInfo(buttons_info),
      std::move(buttons_image));
}

vector<make_button::RenderInfo> Button::CreateMakeButtonRenderInfos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = buttons_info.button_infos.size();
  const float button_height_ndc =
      kNdcDim / static_cast<float>(num_buttons * button::kNumStates);
  float offset_y_ndc = -1.0f + button_height_ndc / 2.0f;
  vector<make_button::RenderInfo> render_infos;
  render_infos.reserve(num_buttons * button::kNumStates);
  for (const auto& info : buttons_info.button_infos) {
    for (int state = 0; state < button::kNumStates; ++state) {
      render_infos.emplace_back(make_button::RenderInfo{
          info.colors[state],
          /*center=*/glm::vec2{0.0f, offset_y_ndc},
      });
      offset_y_ndc += button_height_ndc;
    }
  }
  return render_infos;
}

button::VerticesInfo Button::CreateMakeButtonVerticesInfo(
    int num_buttons, const glm::vec2& background_image_size) const {
  constexpr float kButtonDimensionToIntervalRatio = 100.0f;
  const glm::vec2 interval_candidates =
      background_image_size / kButtonDimensionToIntervalRatio;
  const float button_interval = std::max(interval_candidates.x,
                                         interval_candidates.y);
  const glm::vec2& button_scale =
      background_image_size / (background_image_size + button_interval);
  const float button_height_ndc =
      kNdcDim / static_cast<float>(num_buttons * button::kNumStates);

  button::VerticesInfo vertices_info{};
  SetVerticesPositions(/*size_ndc=*/glm::vec2{kNdcDim, button_height_ndc},
                       button_scale, &vertices_info);
  SetVerticesTexCoords(/*center_uv=*/glm::vec2{kUvDim} / 2.0f,
                       /*size_uv=*/glm::vec2{kUvDim}, &vertices_info);
  return vertices_info;
}

vector<Button::TextPos> Button::CreateMakeButtonTextPos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = buttons_info.button_infos.size();
  const float button_height =
      kUvDim / static_cast<float>(num_buttons * button::kNumStates);
  const float text_height =
      (buttons_info.top_y - buttons_info.base_y) * button_height;

  float offset_y = 0.0f;
  vector<TextPos> text_pos;
  text_pos.reserve(num_buttons);
  for (int button = 0; button < num_buttons; ++button) {
    for (int state = 0; state < button::kNumStates; ++state) {
      const float base_y = offset_y + buttons_info.base_y * button_height;
      text_pos.emplace_back(TextPos{base_y, text_height});
      offset_y += button_height;
    }
  }
  return text_pos;
}

Button::DrawButtonRenderInfos Button::ExtractDrawButtonRenderInfos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = buttons_info.button_infos.size();
  const float button_tex_height =
      kUvDim / static_cast<float>(num_buttons * button::kNumStates);
  constexpr float kTexCenterOffsetX = kUvDim / 2.0f;
  float tex_center_offset_y = button_tex_height / 2.0f;

  // Flip Y for texture centers.
  DrawButtonRenderInfos render_infos;
  render_infos.reserve(num_buttons);
  for (const auto& info : buttons_info.button_infos) {
    const auto& pos_center_ndc = info.center * 2.0f - 1.0f;
    render_infos.emplace_back(
        std::array<draw_button::RenderInfo, button::kNumStates>{
            draw_button::RenderInfo{
                buttons_info.button_alphas[button::kSelectedState],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX, kUvDim - tex_center_offset_y}},
            draw_button::RenderInfo{
                buttons_info.button_alphas[button::kUnselectedState],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX,
                          kUvDim - (tex_center_offset_y + button_tex_height)}},
        });
    tex_center_offset_y += 2 * button_tex_height;
  }
  return render_infos;
}

button::VerticesInfo Button::CreateDrawButtonVerticesInfo(
    const ButtonsInfo& buttons_info) const {
  const glm::vec2 button_size_ndc = buttons_info.button_size * kNdcDim;
  const int num_buttons = buttons_info.button_infos.size();
  const float button_tex_height =
      kUvDim / static_cast<float>(num_buttons * button::kNumStates);

  // Flip Y for texture coordinates.
  button::VerticesInfo vertices_info{};
  SetVerticesPositions(button_size_ndc, /*scale=*/glm::vec2{1.0f},
                       &vertices_info);
  SetVerticesTexCoords(/*center_uv*/glm::vec2{0.0f},
                       /*size_uv=*/glm::vec2{kUvDim, -button_tex_height},
                       &vertices_info);
  return vertices_info;
}

void Button::UpdateFramebuffer(const VkExtent2D& frame_size,
                               VkSampleCountFlagBits sample_count,
                               const RenderPass& render_pass,
                               uint32_t subpass_index) {
  button_renderer_->UpdateFramebuffer(
      sample_count, render_pass, subpass_index,
      pipeline::GetViewport(frame_size, viewport_aspect_ratio_));
}

void Button::Draw(const VkCommandBuffer& command_buffer,
                  absl::Span<const State> button_states) {
  const int num_buttons = all_buttons_.size();
  ASSERT_TRUE(button_states.size() == num_buttons,
              absl::StrFormat("Length of button states (%d) must match with "
                              "number of buttons (%d)",
                              button_states.size(), num_buttons));

  buttons_to_render_.clear();
  for (int button = 0; button < num_buttons; ++button) {
    switch (button_states[button]) {
      case State::kHidden:
        break;
      case State::kSelected:
        buttons_to_render_.emplace_back(
            all_buttons_[button][button::kSelectedState]);
        break;
      case State::kUnselected:
        buttons_to_render_.emplace_back(
            all_buttons_[button][button::kUnselectedState]);
        break;
    }
  }
  if (!buttons_to_render_.empty()) {
    button_renderer_->Draw(command_buffer, buttons_to_render_);
  }
}

absl::optional<int> Button::GetClickedButtonIndex(
    const glm::vec2& click_ndc, int button_index_offset,
    absl::Span<const State> button_states) const {
  const int num_buttons = all_buttons_.size();
  ASSERT_TRUE(button_states.size() == num_buttons,
              absl::StrFormat("Length of button states (%d) must match with "
                              "number of buttons (%d)",
                              button_states.size(), num_buttons));

  for (int i = 0; i < num_buttons; ++i) {
    if (button_states[i] == State::kHidden) {
      continue;
    }
    const glm::vec2 distance =
        glm::abs(click_ndc - all_buttons_[i][0].pos_center_ndc);
    if (distance.x <= button_half_size_ndc_.x &&
        distance.y <= button_half_size_ndc_.y) {
      return button_index_offset + i;
    }
  }
  return absl::nullopt;
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
