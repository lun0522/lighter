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

struct ButtonRenderInfo {
  glm::vec3 color;
  glm::vec2 center;
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

  VerticesInfo& FlipPosY() {
    for (auto& pos_tex_coord : pos_tex_coords_) {
      pos_tex_coord.y *= -1;
    }
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
    const vector<button::ButtonInfo>& button_infos) {
  using button::ButtonInfo;
  const int num_buttons = button_infos.size();
  const float button_height_ndc = 1.0f / static_cast<float>(num_buttons);

  float offset_y = -1.0f + button_height_ndc / 2.0f;
  vector<ButtonRenderInfo> render_infos;
  render_infos.reserve(num_buttons * ButtonInfo::kNumStates);
  for (const auto& info : button_infos) {
    render_infos.emplace_back(ButtonRenderInfo{
        info.colors[ButtonInfo::kUnselected],
        /*center=*/glm::vec2{0.0f, offset_y},
    });
    offset_y += button_height_ndc;
    render_infos.emplace_back(ButtonRenderInfo{
        info.colors[ButtonInfo::kSelected],
        /*center=*/glm::vec2{0.0f, offset_y},
    });
    offset_y += button_height_ndc;
  }
  for (auto& info : render_infos) {
    info.center.y *= -1;
  }

  return absl::make_unique<PerInstanceBuffer>(
      context, render_infos, vector<VertexBuffer::Attribute>{
          {offsetof(ButtonRenderInfo, color), VK_FORMAT_R32G32B32_SFLOAT},
          {offsetof(ButtonRenderInfo, center), VK_FORMAT_R32G32_SFLOAT},
      });
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
      .FlipPosY()
      .set_tex_coord(0, 0.0f, 1.0f)
      .set_tex_coord(1, 0.0f, 0.0f)
      .set_tex_coord(2, 1.0f, 1.0f)
      .set_tex_coord(3, 0.0f, 0.0f)
      .set_tex_coord(4, 1.0f, 0.0f)
      .set_tex_coord(5, 1.0f, 1.0f);
  return push_constant;
}

} /* namespace */

Button::Button(
    const SharedBasicContext& context,
    Text::Font font, int font_height,
    const glm::vec3& text_color, const glm::vec2& button_size,
    const std::array<float, button::ButtonInfo::kNumStates> button_alpha,
    const vector<button::ButtonInfo>& button_infos) {
  const int num_buttons = button_infos.size();
  vector<std::string> texts;
  texts.reserve(num_buttons);
  for (const auto& info : button_infos) {
    texts.emplace_back(info.text);
  }
  DynamicText text_renderer{context, /*num_frames_in_flight=*/1,
                            /*viewport_aspect_ratio=*/1.0f,
                            texts, font, font_height};

  const SharedTexture button_image{
      context, common::file::GetResourcePath("texture/rect_rounded.jpg")};
  buttons_image_ = absl::make_unique<OffscreenImage>(
      context, /*channel=*/4, VkExtent2D{
          button_image->extent().width,
          static_cast<uint32_t>(button_image->extent().height *
                                num_buttons * button::ButtonInfo::kNumStates),
      });

  const auto descriptor = CreateDescriptor(
      context, button_image.GetDescriptorInfo());

  const auto per_instance_buffer =
      CreatePerInstanceBuffer(context, button_infos);

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
      .SetFrontFaceDirection(/*counter_clockwise=*/false)
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

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        pipeline->Bind(command_buffer);
        per_instance_buffer->Bind(
            command_buffer, kPerInstanceBufferBindingPoint);
        push_constant->Flush(
            command_buffer, pipeline->layout(), /*frame=*/0,
            /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
        descriptor->Bind(command_buffer, pipeline->layout());
        vkCmdDraw(
            command_buffer, kNumVerticesPerButton,
            /*instanceCount=*/num_buttons * button::ButtonInfo::kNumStates,
            /*firstVertex=*/0, /*firstInstance=*/0);
      },
      [&](const VkCommandBuffer& command_buffer) {

      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
  });
}

void Button::Draw(const VkCommandBuffer& command_buffer, int frame) const {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
