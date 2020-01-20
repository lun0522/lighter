//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/button.h"

#include <algorithm>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
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

// Helps to set the position part of VerticesInfo.
void SetVerticesPositions(const glm::vec2& size_ndc, const glm::vec2& scale,
                          button::VerticesInfo* info) {
  const auto set_pos = [info, &scale](int index, float x, float y) {
    info->pos_tex_coords[index].x = x * scale.x;
    info->pos_tex_coords[index].y = y * scale.y;
  };
  const glm::vec2 half_size_ndc = size_ndc / 2.0f;
  set_pos(0, -half_size_ndc.x,  half_size_ndc.y);
  set_pos(1, -half_size_ndc.x, -half_size_ndc.y);
  set_pos(2,  half_size_ndc.x,  half_size_ndc.y);
  set_pos(3, -half_size_ndc.x, -half_size_ndc.y);
  set_pos(4,  half_size_ndc.x, -half_size_ndc.y);
  set_pos(5,  half_size_ndc.x,  half_size_ndc.y);
}

// Helps to set the texture coordinate part of VerticesInfo.
void SetVerticesTexCoords(const glm::vec2& center_uv, const glm::vec2& size_uv,
                          button::VerticesInfo* info) {
  const auto set_tex_coord = [info, &center_uv](int index, float x, float y) {
    info->pos_tex_coords[index].z = center_uv.x + x;
    info->pos_tex_coords[index].w = center_uv.y + y;
  };
  const glm::vec2 half_size_uv = size_uv / 2.0f;
  set_tex_coord(0, -half_size_uv.x,  half_size_uv.y);
  set_tex_coord(1, -half_size_uv.x, -half_size_uv.y);
  set_tex_coord(2,  half_size_uv.x,  half_size_uv.y);
  set_tex_coord(3, -half_size_uv.x, -half_size_uv.y);
  set_tex_coord(4,  half_size_uv.x, -half_size_uv.y);
  set_tex_coord(5,  half_size_uv.x,  half_size_uv.y);
}

} /* namespace */

ButtonMaker::ButtonMaker(SharedBasicContext context,
                         Text::Font font, int font_height,
                         vector<std::string>&& texts)
    : context_{std::move(context)}, texts_{std::move(texts)} {
  const int num_buttons = texts_.size();
  const SamplableImage::Config sampler_config{};
  background_image_ = absl::make_unique<SharedTexture>(
      context_, common::file::GetResourcePath("texture/rect_rounded.jpg"),
      sampler_config);
  const VkExtent2D buttons_image_extent{
      (*background_image_)->extent().width,
      static_cast<uint32_t>((*background_image_)->extent().height *
                            num_buttons * state::kNumStates),
  };
  buttons_image_ = absl::make_unique<OffscreenImage>(
      context_, common::kRgbaImageChannel, buttons_image_extent,
      sampler_config);
  text_renderer_ = absl::make_unique<DynamicText>(
      context_, /*num_frames_in_flight=*/1,
      util::GetAspectRatio(buttons_image_->extent()),
      texts_, font, font_height);
}

void ButtonMaker::CreateButtonsImage(
    const button::VerticesInfo& vertices_info, const glm::vec3& text_color,
    const vector<make_button::RenderInfo>& render_infos,
    const vector<make_button::TextPos>& text_pos) {
  enum SubpassIndex {
    kBackgroundSubpassIndex = 0,
    kTextSubpassIndex,
    kNumSubpasses,
    kNumOverlaySubpasses = kNumSubpasses - kBackgroundSubpassIndex,
  };

  const auto per_instance_buffer = absl::make_unique<StaticPerInstanceBuffer>(
      context_, render_infos, RenderInfo::GetAttributes());

  auto push_constant = absl::make_unique<PushConstant>(
      context_, sizeof(button::VerticesInfo), /*num_frames_in_flight=*/1);
  *push_constant->HostData<button::VerticesInfo>(/*frame=*/0) = vertices_info;
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      push_constant->size_per_frame()};

  const auto descriptor =
      CreateDescriptor(background_image_->GetDescriptorInfo());

  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  NaiveRenderPassBuilder render_pass_builder{
      context_, subpass_config, /*num_framebuffers=*/1,
      /*present_to_screen=*/false, /*multisampling_mode=*/absl::nullopt,
  };
  render_pass_builder.mutable_builder()->UpdateAttachmentImage(
      render_pass_builder.color_attachment_index(),
      [this](int) -> const Image& { return *buttons_image_; });
  const auto render_pass = render_pass_builder->Build();

  text_renderer_->Update(
      buttons_image_->extent(), buttons_image_->sample_count(),
      *render_pass, kTextSubpassIndex);
  constexpr float kTextBaseX = kUvDim / 2.0f;
  for (int i = 0; i < text_pos.size(); ++i) {
    text_renderer_->AddText(texts_[i / state::kNumStates], text_pos[i].height,
                            kTextBaseX, text_pos[i].base_y,
                            Text::Align::kCenter);
  }

  const auto pipeline = PipelineBuilder{context_}
      .SetName("make button")
      .AddVertexInput(
          kPerInstanceBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<RenderInfo>(),
          per_instance_buffer->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor->layout()}, {push_constant_range})
      .SetViewport(pipeline::GetFullFrameViewport(buttons_image_->extent()))
      .SetRenderPass(**render_pass, kBackgroundSubpassIndex)
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("make_button.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("make_button.frag"))
      .Build();

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        per_instance_buffer->Bind(
            command_buffer, kPerInstanceBufferBindingPoint, /*offset=*/0);
        push_constant->Flush(
            command_buffer, pipeline->layout(), /*frame=*/0,
            /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
        descriptor->Bind(command_buffer, pipeline->layout());
        VertexBuffer::DrawWithoutBuffer(
            command_buffer, button::kNumVerticesPerButton,
            /*instance_count=*/render_infos.size());
      },
      [&](const VkCommandBuffer& command_buffer) {
        text_renderer_->Draw(command_buffer, /*frame=*/0,
                             text_color, /*alpha=*/1.0f);
      },
  };

  const OneTimeCommand command{context_, &context_->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

std::unique_ptr<StaticDescriptor> ButtonMaker::CreateDescriptor(
    const VkDescriptorImageInfo& image_info) const {
  auto descriptor = absl::make_unique<StaticDescriptor>(
      context_, /*infos=*/vector<Descriptor::Info>{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }});
  descriptor->UpdateImageInfos(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               {{kImageBindingPoint, {image_info}}});
  return descriptor;
}

ButtonRenderer::ButtonRenderer(
    const SharedBasicContext& context,
    int num_buttons, const button::VerticesInfo& vertices_info,
    std::unique_ptr<OffscreenImage>&& buttons_image)
    : buttons_image_{std::move(buttons_image)}, pipeline_builder_{context} {
  per_instance_buffer_ = absl::make_unique<DynamicPerInstanceBuffer>(
      context, sizeof(RenderInfo),
      /*max_num_instances=*/num_buttons * state::kNumStates,
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
      {{kImageBindingPoint, {buttons_image_->GetDescriptorInfo()}}});
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
    const vector<draw_button::RenderInfo>& buttons_to_render) {
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

  vector<std::string> texts;
  texts.reserve(num_buttons);
  for (const auto& info : buttons_info.button_infos) {
    texts.emplace_back(info.text);
  }
  ButtonMaker button_maker{context, buttons_info.font, buttons_info.font_height,
                           std::move(texts)};
  button_maker.CreateButtonsImage(
      CreateMakeButtonVerticesInfo(
          num_buttons, util::ExtentToVec(button_maker.background_image_size())),
      buttons_info.text_color, CreateMakeButtonRenderInfos(buttons_info),
      CreateMakeButtonTextPos(button_maker.max_bearing_y(), buttons_info));

  button_renderer_ = absl::make_unique<ButtonRenderer>(
      context, num_buttons, CreateDrawButtonVerticesInfo(buttons_info),
      button_maker.GetButtonsImage());
}

vector<make_button::RenderInfo> Button::CreateMakeButtonRenderInfos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = buttons_info.button_infos.size();
  const float button_height_ndc =
      kNdcDim / static_cast<float>(num_buttons * state::kNumStates);
  float offset_y_ndc = -1.0f + button_height_ndc / 2.0f;
  vector<make_button::RenderInfo> render_infos;
  render_infos.reserve(num_buttons * state::kNumStates);
  for (const auto& info : buttons_info.button_infos) {
    for (int state = 0; state < state::kNumStates; ++state) {
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
      kNdcDim / static_cast<float>(num_buttons * state::kNumStates);

  button::VerticesInfo vertices_info{};
  SetVerticesPositions(/*size_ndc=*/glm::vec2{kNdcDim, button_height_ndc},
                       button_scale, &vertices_info);
  SetVerticesTexCoords(/*center_uv=*/glm::vec2{kUvDim} / 2.0f,
                       /*size_uv=*/glm::vec2{kUvDim}, &vertices_info);
  return vertices_info;
}

vector<make_button::TextPos> Button::CreateMakeButtonTextPos(
    float max_bearing_y, const ButtonsInfo& buttons_info) const {
  // Y coordinate is flipped.
  const int num_buttons = buttons_info.button_infos.size();
  const float button_height =
      kUvDim / static_cast<float>(num_buttons * state::kNumStates);
  const float text_height = (buttons_info.top_y - buttons_info.base_y) *
                            button_height / max_bearing_y;

  float offset_y = kUvDim;
  vector<make_button::TextPos> text_pos;
  text_pos.reserve(num_buttons);
  for (int button = 0; button < num_buttons; ++button) {
    for (int state = 0; state < state::kNumStates; ++state) {
      offset_y -= button_height;
      const float base_y =
          kUvDim - (offset_y + buttons_info.base_y * button_height);
      text_pos.emplace_back(make_button::TextPos{base_y, -text_height});
    }
  }
  return text_pos;
}

Button::DrawButtonRenderInfos Button::ExtractDrawButtonRenderInfos(
    const ButtonsInfo& buttons_info) const {
  const int num_buttons = buttons_info.button_infos.size();
  const float button_tex_height =
      kUvDim / static_cast<float>(num_buttons * state::kNumStates);
  constexpr float kTexCenterOffsetX = kUvDim / 2.0f;
  float tex_center_offset_y = button_tex_height / 2.0f;

  DrawButtonRenderInfos render_infos;
  render_infos.reserve(num_buttons);
  for (const auto& info : buttons_info.button_infos) {
    const auto& pos_center_ndc = info.center * 2.0f - 1.0f;
    render_infos.emplace_back(
        std::array<draw_button::RenderInfo, state::kNumStates>{
            draw_button::RenderInfo{
                buttons_info.button_alphas[state::kSelected],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX, tex_center_offset_y}},
            draw_button::RenderInfo{
                buttons_info.button_alphas[state::kUnselected],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX,
                          tex_center_offset_y + button_tex_height}},
        });
    tex_center_offset_y += 2 * button_tex_height;
  }
  // Since button maker produces flipped image, we need to flip Y-axis as well.
  for (auto& info : render_infos) {
    info[state::kSelected].tex_coord_center.y =
        kUvDim - info[state::kSelected].tex_coord_center.y;
    info[state::kUnselected].tex_coord_center.y =
        kUvDim - info[state::kUnselected].tex_coord_center.y;
  }
  return render_infos;
}

button::VerticesInfo Button::CreateDrawButtonVerticesInfo(
    const ButtonsInfo& buttons_info) const {
  const glm::vec2 button_size_ndc = buttons_info.button_size * kNdcDim;
  const int num_buttons = buttons_info.button_infos.size();
  const float button_tex_height =
      kUvDim / static_cast<float>(num_buttons * state::kNumStates);

  button::VerticesInfo vertices_info{};
  SetVerticesPositions(button_size_ndc, /*scale=*/glm::vec2{1.0f},
                       &vertices_info);
  SetVerticesTexCoords(/*center_uv*/glm::vec2{0.0f},
                       /*size_uv=*/glm::vec2{kUvDim, button_tex_height},
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
                  const vector<State>& button_states) {
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
        buttons_to_render_.emplace_back(all_buttons_[button][state::kSelected]);
        break;
      case State::kUnselected:
        buttons_to_render_.emplace_back(
            all_buttons_[button][state::kUnselected]);
        break;
    }
  }
  if (!buttons_to_render_.empty()) {
    button_renderer_->Draw(command_buffer, buttons_to_render_);
  }
}

absl::optional<int> Button::GetClickedButtonIndex(
    const glm::vec2& click_ndc, const vector<State>& button_states) const {
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
      return i;
    }
  }
  return absl::nullopt;
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
