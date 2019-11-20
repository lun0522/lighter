//
//  button.cc
//
//  Created by Pujun Lun on 11/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/button.h"

#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/align.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/memory/memory.h"
#include "third_party/absl/strings/str_format.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

using std::vector;

enum SubpassIndex {
  kBackgroundSubpassIndex = 0,
  kTextSubpassIndex,
  kNumSubpasses,
};

enum UniformBindingPoint {
  kVerticesInfoBindingPoint = 0,
  kImageBindingPoint,
};

constexpr int kNumVerticesPerButton = 6;
constexpr uint32_t kPerInstanceBufferBindingPoint = 0;
constexpr int kNumButtonStates = button::ButtonInfo::kNumStates;

struct ButtonRenderInfo {
  glm::vec4 color_alpha;
  glm::vec2 center;
};

struct TextPosition {
  float base_y;
  float height;
};

/* BEGIN: Consistent with uniform blocks defined in shaders. */

class VerticesInfo {
 public:
  VerticesInfo& set_pos(int index, float x, float y) {
    ValidateIndex(index);
    pos_tex_coords_[index].x = x;
    pos_tex_coords_[index].y = y;
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

/* END: Consistent with uniform blocks defined in shaders. */

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

std::unique_ptr<PerInstanceBuffer> CreatePerInstanceBuffer(
    const SharedBasicContext& context,
    const vector<button::ButtonInfo::Info>& button_infos) {
  constexpr float kButtonAlphas[kNumButtonStates]{1.0f, 0.5f};
  const int num_buttons = button_infos.size();
  const float button_height_ndc = 1.0f / static_cast<float>(num_buttons);

  float offset_y_ndc = -1.0f + button_height_ndc / 2.0f;
  vector<ButtonRenderInfo> render_infos;
  render_infos.reserve(num_buttons * kNumButtonStates);
  for (const auto& info : button_infos) {
    for (int state = 0; state < kNumButtonStates; ++state) {
      render_infos.emplace_back(ButtonRenderInfo{
          glm::vec4{info.colors[state], kButtonAlphas[state]},
          /*center=*/glm::vec2{0.0f, offset_y_ndc},
      });
      offset_y_ndc += button_height_ndc;
    }
  }

  vector<VertexBuffer::Attribute> per_instance_attribs {
      {offsetof(ButtonRenderInfo, color_alpha), VK_FORMAT_R32G32B32A32_SFLOAT},
      {offsetof(ButtonRenderInfo, center), VK_FORMAT_R32G32_SFLOAT},
  };
  return absl::make_unique<PerInstanceBuffer>(
      context, render_infos, std::move(per_instance_attribs));
}

std::unique_ptr<PushConstant> CreatePushConstant(
    const SharedBasicContext& context, int num_buttons) {
  auto push_constant = absl::make_unique<PushConstant>(
      context, sizeof(VerticesInfo), /*num_frames_in_flight=*/1);
  const float button_half_height_ndc =
      1.0f / static_cast<float>(num_buttons) / 2.0f;
  (*push_constant->HostData<VerticesInfo>(/*frame=*/0))
      .set_pos(0, -1.0f,  button_half_height_ndc)
      .set_pos(1, -1.0f, -button_half_height_ndc)
      .set_pos(2,  1.0f,  button_half_height_ndc)
      .set_pos(3, -1.0f, -button_half_height_ndc)
      .set_pos(4,  1.0f, -button_half_height_ndc)
      .set_pos(5,  1.0f,  button_half_height_ndc)
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

  float offset_y = 0.0f;
  vector<TextPosition> positions;
  positions.reserve(num_buttons * kNumButtonStates);
  for (int button = 0; button < num_buttons; ++button) {
    for (int state = 0; state < kNumButtonStates; ++state) {
      positions.emplace_back(TextPosition{
          /*base_y=*/offset_y + base_y * button_height,
          text_height,
      });
      offset_y += button_height;
    }
  }
  for (auto& pos : positions) {
    pos.base_y += pos.height;
    pos.height *= -1;
  }

  return positions;
}

} /* namespace */

ButtonMaker::ButtonMaker(const SharedBasicContext& context,
                         const button::ButtonInfo& button_info) {
  const int num_buttons = button_info.button_infos.size();
  const SharedTexture button_image{
      context, common::file::GetResourcePath("texture/rect_rounded.jpg")};
  buttons_image_ = absl::make_unique<OffscreenImage>(
      context, /*channel=*/4, VkExtent2D{
          button_image->extent().width,
          static_cast<uint32_t>(button_image->extent().height *
                                num_buttons * kNumButtonStates),
      });

  const auto descriptor = CreateDescriptor(
      context, button_image.GetDescriptorInfo());

  const auto per_instance_buffer =
      CreatePerInstanceBuffer(context, button_info.button_infos);

  const auto push_constant = CreatePushConstant(context, num_buttons);
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_VERTEX_BIT, /*offset=*/0,
      push_constant->size_per_frame()};

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
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)})
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

  // TODO: Render with different alpha.
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
                           button_info.button_alphas[0]);
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

Button::Button(const SharedBasicContext& context,
               const button::ButtonInfo& button_info)
    : button_maker_{context, button_info} {

}

void Button::Draw(const VkCommandBuffer& command_buffer, int frame) const {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
