//
//  button_maker.cc
//
//  Created by Pujun Lun on 1/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/aurora/editor/button_maker.h"

#include "lighter/application/vulkan/util.h"
#include "lighter/common/data.h"
#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/wrapper/command.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/pipeline_util.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace renderer;
using namespace renderer::vulkan;

enum SubpassIndex {
  kBackgroundSubpassIndex = 0,
  kTextSubpassIndex,
  kNumSubpasses,
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
  const auto image_usages = {
      ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
      ImageUsage::GetSampledInFragmentShaderUsage()};
  return std::make_unique<OffscreenImage>(
      context, buttons_image_extent, common::image::kRgbaImageChannel,
      image_usages, ImageSampler::Config{}, /*use_high_precision=*/false);
}

// Creates per-instance vertex buffer storing RenderInfo.
std::unique_ptr<StaticPerInstanceBuffer> CreatePerInstanceBuffer(
    const SharedBasicContext& context,
    absl::Span<const make_button::ButtonInfo> button_infos) {
  std::vector<make_button::RenderInfo> render_infos;
  render_infos.reserve(button_infos.size() * button::kNumStates);
  for (const auto& info : button_infos) {
    render_infos.push_back({info.render_info[button::kSelectedState]});
    render_infos.push_back({info.render_info[button::kUnselectedState]});
  }
  return std::make_unique<StaticPerInstanceBuffer>(
      context, render_infos,
      pipeline::GetVertexAttributes<make_button::RenderInfo>());
}

// Returns a descriptor with an image bound to it.
std::unique_ptr<StaticDescriptor> CreateDescriptor(
    const SharedBasicContext& context,
    const VkDescriptorImageInfo& image_info) {
  auto descriptor = std::make_unique<StaticDescriptor>(
      context, /*infos=*/std::vector<Descriptor::Info>{
          Descriptor::Info{
              Image::GetDescriptorTypeForSampling(),
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{{kImageBindingPoint, /*array_length=*/1}},
          }});
  descriptor->UpdateImageInfos(
      Image::GetDescriptorTypeForSampling(),
      /*image_info_map=*/{{kImageBindingPoint, {image_info}}});
  return descriptor;
}

// Creates a render pass for rendering to 'target_image'.
std::unique_ptr<RenderPass> CreateRenderPass(
    const SharedBasicContext& context, const OffscreenImage& target_image) {
  ImageUsageHistory usage_history{target_image.GetInitialUsage()};
  usage_history
      .AddUsage(kBackgroundSubpassIndex, kTextSubpassIndex,
                ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0))
      .SetFinalUsage(ImageUsage::GetSampledInFragmentShaderUsage());

  GraphicsPass graphics_pass{context, kNumSubpasses};
  graphics_pass.AddAttachment("Button", std::move(usage_history),
                              /*get_location=*/[](int subpass) { return 0; });

  auto render_pass_builder =
      graphics_pass.CreateRenderPassBuilder(/*num_framebuffers=*/1);
  render_pass_builder->UpdateAttachmentImage(
      /*index=*/0,
      [&target_image](int) -> const Image& { return target_image; });
  return render_pass_builder->Build();
}

// Creates a text renderer for rendering texts on buttons.
std::unique_ptr<DynamicText> CreateTextRenderer(
    const SharedBasicContext& context, Text::Font font, int font_height,
    const Image& target_image, const RenderPass& render_pass,
    absl::Span<const make_button::ButtonInfo> button_infos) {
  std::vector<std::string> texts;
  texts.reserve(button_infos.size());
  for (const auto& info : button_infos) {
    texts.push_back(info.text);
  }

  auto text_renderer = std::make_unique<DynamicText>(
      context, /*num_frames_in_flight=*/1,
      renderer::vulkan::util::GetAspectRatio(target_image.extent()),
      texts, font, font_height);
  text_renderer->Update(
      target_image.extent(), target_image.sample_count(),
      render_pass, kTextSubpassIndex, /*flip_y=*/false);

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

namespace make_button {

std::vector<common::VertexAttribute> RenderInfo::GetVertexAttributes() {
  std::vector<common::VertexAttribute> attributes;
  common::data::AppendVertexAttributes<glm::vec3>(
      attributes, offsetof(RenderInfo, color));
  common::data::AppendVertexAttributes<glm::vec2>(
      attributes, offsetof(RenderInfo, center));
  return attributes;
}

} /* namespace make_button */

std::unique_ptr<OffscreenImage> ButtonMaker::CreateButtonsImage(
    const SharedBasicContext& context, Text::Font font, int font_height,
    const glm::vec3& text_color, const common::Image& button_background,
    const button::VerticesInfo& vertices_info,
    absl::Span<const make_button::ButtonInfo> button_infos) {
  ASSERT_TRUE(button_background.channel() == common::image::kBwImageChannel,
              "Expecting a single-channel button background image");
  const auto image_usages = {ImageUsage::GetSampledInFragmentShaderUsage()};
  const auto background_image = std::make_unique<TextureImage>(
      context, /*generate_mipmaps=*/false, button_background, image_usages,
      ImageSampler::Config{});

  const int num_buttons = button_infos.size();
  auto buttons_image = CreateTargetImage(context, num_buttons,
                                         background_image->extent());

  const auto per_instance_buffer =
      CreatePerInstanceBuffer(context, button_infos);

  const auto push_constant = std::make_unique<PushConstant>(
      context, sizeof(button::VerticesInfo), /*num_frames_in_flight=*/1);
  *push_constant->HostData<button::VerticesInfo>(/*frame=*/0) = vertices_info;

  const auto descriptor = CreateDescriptor(
      context, background_image->GetDescriptorInfoForSampling());

  const auto render_pass = CreateRenderPass(context, *buttons_image);

  const auto text_renderer = CreateTextRenderer(
      context, font, font_height, *buttons_image, *render_pass, button_infos);

  const auto pipeline = GraphicsPipelineBuilder{context}
      .SetPipelineName("Button background")
      .AddVertexInput(
          kPerInstanceBufferBindingPoint,
          pipeline::GetPerInstanceBindingDescription<make_button::RenderInfo>(),
          per_instance_buffer->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(
          {descriptor->layout()},
          {push_constant->MakePerFrameRange(VK_SHADER_STAGE_VERTEX_BIT)})
      .SetViewport(pipeline::GetFullFrameViewport(buttons_image->extent()),
                   /*flip_y=*/false)
      .SetRenderPass(**render_pass, kBackgroundSubpassIndex)
      .SetColorBlend(
          {pipeline::GetColorAlphaBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 GetShaderBinaryPath("aurora/make_button.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 GetShaderBinaryPath("aurora/make_button.frag"))
      .Build();

  const std::vector<RenderPass::RenderOp> render_ops{
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
} /* namespace lighter */
