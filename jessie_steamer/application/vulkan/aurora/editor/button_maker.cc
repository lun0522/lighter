//
//  button_maker.cc
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/editor/button_maker.h"

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image_util.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/pipeline_util.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/memory/memory.h"

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
  kNumOverlaySubpasses = kNumSubpasses - kBackgroundSubpassIndex,
};

enum UniformBindingPoint {
  kVerticesInfoBindingPoint = 0,
  kImageBindingPoint,
};

constexpr float kUvDim = 1.0f;
constexpr uint32_t kPerInstanceBufferBindingPoint = 0;

// Creates a big offscreen image. We will render all buttons in all states onto
// this image.
std::unique_ptr<OffscreenImage> CreateTargetImage(
    const SharedBasicContext& context,
    int num_buttons, const VkExtent2D& background_image_extent) {
  const VkExtent2D buttons_image_extent{
      background_image_extent.width,
      static_cast<uint32_t>(background_image_extent.height *
                            num_buttons * button::kNumStates),
  };
  const auto image_usage_flags = image::UsageInfo{"Buttons image"}
      .SetInitialUsage(image::Usage::kRenderingTarget)
      .SetFinalUsage(image::Usage::kSampledInFragmentShader)
      .GetImageUsageFlags();
  return absl::make_unique<OffscreenImage>(
      context, buttons_image_extent, common::kRgbaImageChannel,
      image_usage_flags, ImageSampler::Config{});
}

// Creates per-instance vertex buffer storing RenderInfo.
std::unique_ptr<StaticPerInstanceBuffer> CreatePerInstanceBuffer(
    const SharedBasicContext& context,
    absl::Span<const make_button::ButtonInfo> button_infos) {
  vector<ButtonMaker::RenderInfo> render_infos;
  render_infos.reserve(button_infos.size() * button::kNumStates);
  for (const auto& info : button_infos) {
    render_infos.emplace_back(info.render_info[button::kSelectedState]);
    render_infos.emplace_back(info.render_info[button::kUnselectedState]);
  }
  return absl::make_unique<StaticPerInstanceBuffer>(
      context, render_infos, ButtonMaker::RenderInfo::GetAttributes());
}

// Returns a descriptor with an image bound to it.
std::unique_ptr<StaticDescriptor> CreateDescriptor(
    const SharedBasicContext& context,
    const VkDescriptorImageInfo& image_info) {
  auto descriptor = absl::make_unique<StaticDescriptor>(
      context, /*infos=*/vector<Descriptor::Info>{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }});
  descriptor->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*image_info_map=*/{{kImageBindingPoint, {image_info}}});
  return descriptor;
}

// Creates a render pass for rendering to 'target_image'.
std::unique_ptr<RenderPass> CreateRenderPass(
    const SharedBasicContext& context, const OffscreenImage& target_image) {
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  NaiveRenderPassBuilder render_pass_builder{
      context, subpass_config, /*num_framebuffers=*/1,
      /*use_multisampling=*/false,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kSampledAsTexture,
  };
  render_pass_builder.mutable_builder()->UpdateAttachmentImage(
      render_pass_builder.color_attachment_index(),
      [&target_image](int) -> const Image& { return target_image; });
  return render_pass_builder->Build();
}

// Creates a text renderer for rendering texts on buttons.
std::unique_ptr<DynamicText> CreateTextRenderer(
    const SharedBasicContext& context, Text::Font font, int font_height,
    const Image& target_image, const RenderPass& render_pass,
    absl::Span<const make_button::ButtonInfo> button_infos) {
  vector<std::string> texts;
  texts.reserve(button_infos.size());
  for (const auto& info : button_infos) {
    texts.emplace_back(info.text);
  }

  auto text_renderer = absl::make_unique<DynamicText>(
      context, /*num_frames_in_flight=*/1,
      util::GetAspectRatio(target_image.extent()), texts, font, font_height);
  text_renderer->Update(
      target_image.extent(), target_image.sample_count(),
      render_pass, kTextSubpassIndex);

  constexpr float kTextBaseX = kUvDim / 2.0f;
  for (const auto& info : button_infos) {
    for (int state = 0; state < button::kNumStates; ++state) {
      text_renderer->AddText(info.text, info.height[state], kTextBaseX,
                             info.base_y[state], Text::Align::kCenter);
    }
  }

  return text_renderer;
}

} /* namespace */

std::unique_ptr<OffscreenImage> ButtonMaker::CreateButtonsImage(
    const SharedBasicContext& context, Text::Font font, int font_height,
    const glm::vec3& text_color, const common::Image& button_background,
    const button::VerticesInfo& vertices_info,
    absl::Span<const make_button::ButtonInfo> button_infos) {
  ASSERT_TRUE(button_background.channel == common::kBwImageChannel,
              "Expecting a single-channel button background image");
  const auto background_image = absl::make_unique<TextureImage>(
      context, /*generate_mipmaps=*/false, button_background,
      ImageSampler::Config{});

  const int num_buttons = button_infos.size();
  auto buttons_image = CreateTargetImage(context, num_buttons,
                                         background_image->extent());

  const auto per_instance_buffer =
      CreatePerInstanceBuffer(context, button_infos);

  const auto push_constant = absl::make_unique<PushConstant>(
      context, sizeof(button::VerticesInfo), /*num_frames_in_flight=*/1);
  *push_constant->HostData<button::VerticesInfo>(/*frame=*/0) = vertices_info;

  const auto descriptor = CreateDescriptor(
      context, background_image->GetDescriptorInfo());

  const auto render_pass = CreateRenderPass(context, *buttons_image);

  const auto text_renderer = CreateTextRenderer(
      context, font, font_height, *buttons_image, *render_pass, button_infos);

  const auto pipeline = GraphicsPipelineBuilder{context}
      .SetPipelineName("Button background")
      .AddVertexInput(
          kPerInstanceBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<RenderInfo>(),
          per_instance_buffer->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(
          {descriptor->layout()},
          {push_constant->MakePerFrameRange(VK_SHADER_STAGE_VERTEX_BIT)})
      .SetViewport(pipeline::GetFullFrameViewport(buttons_image->extent()))
      .SetRenderPass(**render_pass, kBackgroundSubpassIndex)
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("aurora/make_button.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("aurora/make_button.frag"))
      .Build();

  const vector<RenderPass::RenderOp> render_ops{
      [&](const VkCommandBuffer& command_buffer) {
        // Render buttons' background.
        pipeline->Bind(command_buffer);
        per_instance_buffer->Bind(
            command_buffer, kPerInstanceBufferBindingPoint, /*offset=*/0);
        push_constant->Flush(
            command_buffer, pipeline->layout(), /*frame=*/0,
            /*target_offset=*/0, VK_SHADER_STAGE_VERTEX_BIT);
        descriptor->Bind(command_buffer, pipeline->layout(),
                         pipeline->binding_point());
        VertexBuffer::DrawWithoutBuffer(
            command_buffer, button::kNumVerticesPerButton,
            /*instance_count=*/num_buttons * button::kNumStates);
      },
      [&text_renderer, &text_color](const VkCommandBuffer& command_buffer) {
        // Render texts on buttons.
        text_renderer->Draw(command_buffer, /*frame=*/0,
                            text_color, /*alpha=*/1.0f);
      },
  };

  const OneTimeCommand command{context, &context->queues().graphics_queue()};
  command.Run(
      [&render_pass, &render_ops](const VkCommandBuffer& command_buffer) {
        render_pass->Run(command_buffer, /*framebuffer_index=*/0, render_ops);
      });

  return buttons_image;
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
