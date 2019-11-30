//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/button.h"

#include <algorithm>

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
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
constexpr int kNumVerticesPerButton = 6;
constexpr int kNumButtonStates = button::ButtonInfo::kNumStates;
constexpr uint32_t kPerInstanceBufferBindingPoint = 0;

/* BEGIN: Consistent with vertex input attributes defined in shaders. */

class VerticesInfo {
 public:
  VerticesInfo& set_pos(int index, float x, float y) {
    ValidateIndex(index);
    pos_tex_coords_[index].x = x;
    pos_tex_coords_[index].y = y;
    return *this;
  }

  VerticesInfo& scale_all_pos(const glm::vec2& ratio) {
    for (auto& pos_tex_coord : pos_tex_coords_) {
      pos_tex_coord.x *= ratio.x;
      pos_tex_coord.y *= ratio.y;
    }
    return *this;
  }

  VerticesInfo& set_tex_coord(int index, float x, float y) {
    ValidateIndex(index);
    pos_tex_coords_[index].z = x;
    pos_tex_coords_[index].w = y;
    return *this;
  }

 private:
  static void ValidateIndex(int index) {
    ASSERT_TRUE(index < kNumVerticesPerButton,
                absl::StrFormat("Index (%d) out of range", index));
  }

  ALIGN_VEC4 glm::vec4 pos_tex_coords_[kNumVerticesPerButton];
};

/* END: Consistent with vertex input attributes defined in shaders. */

std::unique_ptr<StaticDescriptor> CreateDescriptor(
    const SharedBasicContext& context,
    const VkDescriptorImageInfo& image_info) {
  const vector<Descriptor::Info> descriptor_infos{
      Descriptor::Info{
          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          VK_SHADER_STAGE_FRAGMENT_BIT,
          /*bindings=*/{
              Descriptor::Info::Binding{
                  kImageBindingPoint,
                  /*array_length=*/1,
              }},
      }};
  auto descriptor = absl::make_unique<StaticDescriptor>(
      context, descriptor_infos);
  descriptor->UpdateImageInfos(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               {{kImageBindingPoint, {image_info}}});
  return descriptor;
}

} /* namespace */

namespace make_button {
namespace {

enum SubpassIndex {
  kBackgroundSubpassIndex = 0,
  kTextSubpassIndex,
  kNumSubpasses,
};

struct ButtonRenderInfo {
  glm::vec3 color;
  glm::vec2 center;
};

struct TextPosition {
  float base_y;
  float height;
};

std::unique_ptr<PerInstanceBuffer> CreatePerInstanceBuffer(
    const SharedBasicContext& context,
    const vector<button::ButtonInfo::Info>& button_infos) {
  const int num_buttons = button_infos.size();
  const float button_height_ndc = 1.0f / static_cast<float>(num_buttons);

  float offset_y_ndc = -1.0f + button_height_ndc / 2.0f;
  vector<ButtonRenderInfo> render_infos;
  render_infos.reserve(num_buttons * kNumButtonStates);
  for (const auto& info : button_infos) {
    for (int state = 0; state < kNumButtonStates; ++state) {
      render_infos.emplace_back(ButtonRenderInfo{
          info.colors[state],
          /*center=*/glm::vec2{0.0f, offset_y_ndc},
      });
      offset_y_ndc += button_height_ndc;
    }
  }

  vector<VertexBuffer::Attribute> per_instance_attribs {
      {offsetof(ButtonRenderInfo, color), VK_FORMAT_R32G32B32_SFLOAT},
      {offsetof(ButtonRenderInfo, center), VK_FORMAT_R32G32_SFLOAT},
  };
  return absl::make_unique<StaticPerInstanceBuffer>(
      context, render_infos, std::move(per_instance_attribs));
}

std::unique_ptr<PushConstant> CreateButtonVerticesInfo(
    const SharedBasicContext& context,
    int num_buttons, const glm::vec2& button_image_size) {
  constexpr float kButtonDimensionToIntervalRatio = 100.0f;
  const glm::vec2 button_interval =
      button_image_size / kButtonDimensionToIntervalRatio;
  const glm::vec2& button_scale =
      button_image_size /
      (button_image_size + std::max(button_interval.x, button_interval.y));
  const float button_half_height_ndc =
      kNdcDim / static_cast<float>(num_buttons * kNumButtonStates) / 2.0f;

  auto push_constant = absl::make_unique<PushConstant>(
      context, sizeof(VerticesInfo), /*num_frames_in_flight=*/1);
  (*push_constant->HostData<VerticesInfo>(/*frame=*/0))
      .set_pos(0, -1.0f,  button_half_height_ndc)
      .set_pos(1, -1.0f, -button_half_height_ndc)
      .set_pos(2,  1.0f,  button_half_height_ndc)
      .set_pos(3, -1.0f, -button_half_height_ndc)
      .set_pos(4,  1.0f, -button_half_height_ndc)
      .set_pos(5,  1.0f,  button_half_height_ndc)
      .scale_all_pos(button_scale)
      .set_tex_coord(0, 0.0f, 1.0f)
      .set_tex_coord(1, 0.0f, 0.0f)
      .set_tex_coord(2, 1.0f, 1.0f)
      .set_tex_coord(3, 0.0f, 0.0f)
      .set_tex_coord(4, 1.0f, 0.0f)
      .set_tex_coord(5, 1.0f, 1.0f);
  return push_constant;
}

vector<TextPosition> CreateTextPositions(int num_buttons, float base_y,
                                         float top_y, float max_bearing_y) {
  const float button_height =
      1.0f / static_cast<float>(num_buttons * kNumButtonStates);
  const float text_height = (top_y - base_y) * button_height / max_bearing_y;

  float offset_y = 1.0f;
  vector<TextPosition> positions;
  positions.reserve(num_buttons * kNumButtonStates);
  for (int button = 0; button < num_buttons; ++button) {
    for (int state = 0; state < kNumButtonStates; ++state) {
      offset_y -= button_height;
      positions.emplace_back(TextPosition{
          /*base_y=*/offset_y + base_y * button_height,
          text_height,
      });
    }
  }
  // Flip Y coordinate.
  for (auto& pos : positions) {
    pos.base_y = 1.0f - pos.base_y;
    pos.height *= -1;
  }

  return positions;
}

} /* namespace */
} /* namespace make_button */

ButtonMaker::ButtonMaker(const SharedBasicContext& context,
                         const ButtonInfo& button_info) {
  using namespace make_button;

  const int num_buttons = button_info.button_infos.size();
  const SamplableImage::Config sampler_config{};
  const SharedTexture button_image{
      context, common::file::GetResourcePath("texture/rect_rounded.jpg"),
      sampler_config};
  const VkExtent2D buttons_image_extent{
      button_image->extent().width,
      static_cast<uint32_t>(button_image->extent().height *
                            num_buttons * kNumButtonStates),
  };
  buttons_image_ = absl::make_unique<OffscreenImage>(
      context, /*channel=*/4, buttons_image_extent, sampler_config);

  const auto per_instance_buffer =
      CreatePerInstanceBuffer(context, button_info.button_infos);

  const auto push_constant = CreateButtonVerticesInfo(
      context, num_buttons, util::ExtentToVec(button_image->extent()));
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      push_constant->size_per_frame()};

  const auto descriptor = CreateDescriptor(
      context, button_image.GetDescriptorInfo());

  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/kNumSubpasses,
  };
  NaiveRenderPassBuilder render_pass_builder{
      context, subpass_config, /*num_framebuffers=*/1,
      /*present_to_screen=*/false, /*multisampling_mode=*/absl::nullopt,
  };
  render_pass_builder.mutable_builder()->UpdateAttachmentImage(
      render_pass_builder.color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return *buttons_image_;
      });
  const auto render_pass = render_pass_builder->Build();

  const auto pipeline = PipelineBuilder{context}
      .SetName("make button")
      .AddVertexInput(
          kPerInstanceBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<ButtonRenderInfo>(),
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

  vector<std::string> texts;
  texts.reserve(num_buttons);
  for (const auto& info : button_info.button_infos) {
    texts.emplace_back(info.text);
  }
  DynamicText text_renderer{
      context, /*num_frames_in_flight=*/1,
      util::GetAspectRatio(buttons_image_->extent()),
      texts, button_info.font, button_info.font_height};
  text_renderer.Update(buttons_image_->extent(), buttons_image_->sample_count(),
                       *render_pass, kTextSubpassIndex);
  const auto text_positions = CreateTextPositions(
      num_buttons, button_info.base_y, button_info.top_y,
      text_renderer.GetMaxBearingY());

  int render_count = 0;
  for (int button = 0; button < num_buttons; ++button) {
    for (int state = 0; state < kNumButtonStates; ++state) {
      const auto& position = text_positions[render_count++];
      text_renderer.AddText(
          button_info.button_infos[button].text, position.height,
          /*base_x=*/0.5f, position.base_y, Text::Align::kCenter);
    }
  }

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        per_instance_buffer->Bind(
            command_buffer, kPerInstanceBufferBindingPoint);
        push_constant->Flush(
            command_buffer, pipeline->layout(), /*frame=*/0,
            /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
        descriptor->Bind(command_buffer, pipeline->layout());
        VertexBuffer::DrawWithoutBuffer(
            command_buffer, kNumVerticesPerButton,
            /*instance_count=*/num_buttons * kNumButtonStates);
      },
      [&](const VkCommandBuffer& command_buffer) {
        text_renderer.Draw(command_buffer, /*frame=*/0, button_info.text_color,
                           /*alpha=*/1.0f);
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

Button::Button(SharedBasicContext context,
               float viewport_aspect_ratio,
               const ButtonInfo& button_info)
    : context_{std::move(context)},
      viewport_aspect_ratio_{viewport_aspect_ratio},
      button_half_size_ndc_{button_info.button_size * kNdcDim / 2.0f},
      all_buttons_{ExtractRenderInfos(button_info)},
      button_maker_{context_, button_info},
      pipeline_builder_{context_} {
  const int num_buttons = button_info.button_infos.size();
  buttons_to_render_.reserve(num_buttons);

  descriptor_ = CreateDescriptor(
      context_, button_maker_.buttons_image()->GetDescriptorInfo());

  per_instance_buffer_ = absl::make_unique<DynamicPerInstanceBuffer>(
      context_, sizeof(ButtonRenderInfo),
      /*max_num_instances=*/num_buttons * kNumButtonStates,
      /*attributes=*/vector<VertexBuffer::Attribute>{
          {offsetof(ButtonRenderInfo, alpha), VK_FORMAT_R32_SFLOAT},
          {offsetof(ButtonRenderInfo, pos_center_ndc), VK_FORMAT_R32G32_SFLOAT},
          {offsetof(ButtonRenderInfo, tex_coord_center),
           VK_FORMAT_R32G32_SFLOAT},
      });

  push_constant_ = CreateButtonVerticesInfo(context_, button_info);
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      push_constant_->size_per_frame()};

  pipeline_builder_
      .SetName("draw button")
      .AddVertexInput(
          kPerInstanceBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<ButtonRenderInfo>(),
          per_instance_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_->layout()}, {push_constant_range})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("draw_button.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("draw_button.frag"));
}

Button::ButtonRenderInfos Button::ExtractRenderInfos(
    const ButtonInfo& button_info) const {
  const int num_buttons = button_info.button_infos.size();
  const float button_tex_height =
      1.0f / static_cast<float>(num_buttons * kNumButtonStates);
  constexpr float kTexCenterOffsetX = 1.0f / 2.0f;
  float tex_center_offset_y = button_tex_height / 2.0f;

  ButtonRenderInfos render_infos;
  render_infos.reserve(num_buttons);
  for (const auto& info : button_info.button_infos) {
    const auto& pos_center_ndc = info.center * 2.0f - 1.0f;
    render_infos.emplace_back(
        std::array<ButtonRenderInfo, ButtonInfo::kNumStates>{
            ButtonRenderInfo{
                button_info.button_alphas[ButtonInfo::kSelected],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX, tex_center_offset_y}},
            ButtonRenderInfo{
                button_info.button_alphas[ButtonInfo::kUnselected],
                pos_center_ndc,
                glm::vec2{kTexCenterOffsetX,
                          tex_center_offset_y + button_tex_height}},
        });
    tex_center_offset_y += 2 * button_tex_height;
  }
  for (auto& info : render_infos) {
    info[ButtonInfo::kSelected].tex_coord_center.y =
        1.0f - info[ButtonInfo::kSelected].tex_coord_center.y;
    info[ButtonInfo::kUnselected].tex_coord_center.y =
        1.0f - info[ButtonInfo::kUnselected].tex_coord_center.y;
  }
  return render_infos;
}

std::unique_ptr<PushConstant> Button::CreateButtonVerticesInfo(
    const SharedBasicContext& context, const ButtonInfo& button_info) const {
  const glm::vec2 button_pos_half_size_ndc =
      button_info.button_size * kNdcDim / 2.0f;
  const int num_buttons = button_info.button_infos.size();
  constexpr float kButtonTexHalfWidth = 1.0f / 2.0f;
  const float button_tex_half_height =
      1.0f / static_cast<float>(num_buttons * kNumButtonStates) / 2.0f;

  auto push_constant = absl::make_unique<PushConstant>(
      context, sizeof(VerticesInfo), /*num_frames_in_flight=*/1);
  (*push_constant->HostData<VerticesInfo>(/*frame=*/0))
      .set_pos(0, -button_pos_half_size_ndc.x,  button_pos_half_size_ndc.y)
      .set_pos(1, -button_pos_half_size_ndc.x, -button_pos_half_size_ndc.y)
      .set_pos(2,  button_pos_half_size_ndc.x,  button_pos_half_size_ndc.y)
      .set_pos(3, -button_pos_half_size_ndc.x, -button_pos_half_size_ndc.y)
      .set_pos(4,  button_pos_half_size_ndc.x, -button_pos_half_size_ndc.y)
      .set_pos(5,  button_pos_half_size_ndc.x,  button_pos_half_size_ndc.y)
      .set_tex_coord(0, -kButtonTexHalfWidth,  button_tex_half_height)
      .set_tex_coord(1, -kButtonTexHalfWidth, -button_tex_half_height)
      .set_tex_coord(2,  kButtonTexHalfWidth,  button_tex_half_height)
      .set_tex_coord(3, -kButtonTexHalfWidth, -button_tex_half_height)
      .set_tex_coord(4,  kButtonTexHalfWidth, -button_tex_half_height)
      .set_tex_coord(5,  kButtonTexHalfWidth,  button_tex_half_height);
  return push_constant;
}

void Button::Update(const VkExtent2D& frame_size,
                    VkSampleCountFlagBits sample_count,
                    const RenderPass& render_pass, uint32_t subpass_index) {
  pipeline_ = pipeline_builder_
      .SetMultisampling(sample_count)
      .SetViewport(pipeline::GetViewport(frame_size, viewport_aspect_ratio_))
      .SetRenderPass(*render_pass, subpass_index)
      .SetColorBlend(
          vector<VkPipelineColorBlendAttachmentState>(
              render_pass.num_color_attachments(subpass_index),
              pipeline::GetColorBlendState(/*enable_blend=*/true)))
      .Build();
}

void Button::Draw(const VkCommandBuffer& command_buffer,
                  const vector<State>& button_states) {
  const int num_buttons = all_buttons_.size();
  ASSERT_NON_NULL(pipeline_, "Update() must have been called");
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
            all_buttons_[button][ButtonInfo::kSelected]);
        break;
      case State::kUnselected:
        buttons_to_render_.emplace_back(
            all_buttons_[button][ButtonInfo::kUnselected]);
        break;
    }
  }
  if (buttons_to_render_.empty()) {
    return;
  }
  per_instance_buffer_->CopyHostData(buttons_to_render_);

  pipeline_->Bind(command_buffer);
  per_instance_buffer_->Bind(
      command_buffer, kPerInstanceBufferBindingPoint);
  push_constant_->Flush(
      command_buffer, pipeline_->layout(), /*frame=*/0,
      /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
  descriptor_->Bind(command_buffer, pipeline_->layout());
  VertexBuffer::DrawWithoutBuffer(command_buffer, kNumVerticesPerButton,
                                  buttons_to_render_.size());
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
