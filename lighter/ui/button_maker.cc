#include "lighter/ui/button_maker.h"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

#include "lighter/base/file.h"
#include "lighter/base/image.h"
#include "lighter/base/util.h"
#include "lighter/renderer/ir/buffer_usage.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/pipeline_util.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::ui {
namespace {

using renderer::ir::AttachmentLoadOp;
using renderer::ir::AttachmentStoreOp;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::DataFormat;
using renderer::ir::FilterType;
using renderer::ir::GraphicsPipelineDescriptor;
using renderer::ir::Image;
using renderer::ir::ImageHandle;
using renderer::ir::ImageUsage;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;
using renderer::ir::SamplerAddressMode;
using renderer::ir::SamplerDescriptor;
using renderer::ir::VertexBufferView;
using renderer::ir::VertexInputRate;

constexpr float kUvDim = 1.0f;
constexpr uint32_t kPerInstanceBufferBindingPoint = 0;
constexpr uint32_t kImageBindingPoint = 1;

}  // namespace

namespace make_button {

std::vector<VertexBufferView::Attribute> RenderInfo::GetVertexAttributes() {
  return {
      {/*location=*/0, DataFormat::kSFloat32Vec3, offsetof(RenderInfo, color)},
      {/*location=*/1, DataFormat::kSFloat32Vec2, offsetof(RenderInfo, center)},
  };
}

}  // namespace make_button

std::unique_ptr<Image> ButtonMaker::CreateButtonsImage(
    Renderer& renderer, Text::Font font, int font_height,
    const glm::vec3& text_color, const base::Image& button_background,
    const button::VerticesInfo& vertices_info,
    std::span<const make_button::ButtonInfo> button_infos) {
  ASSERT_TRUE(button_background.channel() == base::image::kBwImageChannel,
              "Expecting a single-channel button background image");
  const auto background_image = renderer.CreateColorImage(
      "ButtonBackground", button_background, /*generate_mipmaps=*/false,
      std::array{ImageUsage::GetSampledInFragmentShaderUsage()});

  const int num_buttons = static_cast<int>(button_infos.size());
  const glm::ivec2 buttons_extent{
      background_image->width(),
      background_image->height() * num_buttons * button::kNumStates,
  };
  auto buttons_image = renderer.CreateColorImage(
      "ButtonsAtlas", {buttons_extent, base::image::kRgbaImageChannel},
      MultisamplingMode::kNone, /*high_precision=*/false,
      std::array{
          ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0),
          ImageUsage::GetSampledInFragmentShaderUsage(),
      });

  std::vector<make_button::RenderInfo> render_infos;
  render_infos.reserve(button_infos.size() * button::kNumStates);
  std::vector<std::string> texts;
  texts.reserve(button_infos.size());
  for (const auto& info : button_infos) {
    render_infos.push_back(info.render_info[button::kSelectedState]);
    render_infos.push_back(info.render_info[button::kUnselectedState]);
    texts.push_back(info.text);
  }

  const size_t instance_bytes = base::util::GetTotalDataSize(
      std::span<const make_button::RenderInfo>{render_infos});
  const auto per_instance_buffer =
      renderer.CreateBuffer(Buffer::UpdateRate::kLow, instance_bytes,
                            std::array{BufferUsage::GetVertexBufferUsage(
                                BufferUsage::UsageType::kVertexOnly)});
  per_instance_buffer->CopyToDevice(std::array{
      Buffer::CopyInfo{render_infos.data(), instance_bytes, /*offset=*/0}});

  const float aspect_ratio = static_cast<float>(buttons_extent.x) /
                             static_cast<float>(buttons_extent.y);
  DynamicText text_renderer{renderer,
                            aspect_ratio,
                            texts,
                            font,
                            font_height,
                            /*num_frames_in_flight=*/1};
  constexpr float kTextBaseX = kUvDim / 2.0f;
  for (const auto& info : button_infos) {
    for (int state = 0; state < button::kNumStates; ++state) {
      text_renderer.AddText(info.text, info.height[state], kTextBaseX,
                            info.base_y[state], Text::Align::kCenter);
    }
  }

  const GraphicsPipelineDescriptor pipeline_desc =
      GraphicsPipelineDescriptor{}
          .SetName("ButtonBackground")
          .SetShader(
              renderer::ir::shader_stage::VERTEX,
              base::file::GetShaderBinaryPath("shared/button/make_button.vert"))
          .SetShader(
              renderer::ir::shader_stage::FRAGMENT,
              base::file::GetShaderBinaryPath("shared/button/make_button.frag"))
          .UseColorAttachment(/*location=*/0, std::nullopt)
          .SetViewport(
              renderer::ir::pipeline::GetFullFrameViewport(buttons_extent),
              renderer::ir::pipeline::GetFullFrameScissor(buttons_extent),
              /*flip_y=*/false)
          .AddVertexInput({
              VertexInputRate::kInstance,
              kPerInstanceBufferBindingPoint,
              sizeof(make_button::RenderInfo),
              make_button::RenderInfo::GetVertexAttributes(),
          });

  const SamplerDescriptor background_sampler{
      .filter_type = FilterType::kLinear,
      .address_mode = SamplerAddressMode::kClampToEdge,
  };

  RenderGraph graph;
  const ImageHandle target_handle =
      graph.ImportImage("ButtonsAtlas", *buttons_image, ImageUsage{},
                        ImageUsage::GetSampledInFragmentShaderUsage());
  graph.AddGraphicsPass(
      "BakeButtonsAtlas",
      [&](PassBuilder& builder) {
        builder.ColorAttachment(target_handle, /*location=*/0,
                                AttachmentLoadOp::kClear,
                                AttachmentStoreOp::kStore);
      },
      [&](PassContext& ctx) {
        ctx.BindGraphicsPipeline(pipeline_desc);
        ctx.BindVertexBuffer(kPerInstanceBufferBindingPoint,
                             *per_instance_buffer);
        ctx.PushConstants(renderer::ir::shader_stage::VERTEX, /*offset=*/0,
                          sizeof(button::VerticesInfo), &vertices_info);
        ctx.BindExternalImage(kImageBindingPoint, *background_image,
                              &background_sampler);
        ctx.Draw(button::kNumVerticesPerButton,
                 static_cast<uint32_t>(num_buttons * button::kNumStates));

        text_renderer.Draw(ctx, buttons_extent, text_color, /*alpha=*/1.0f,
                           /*flip_y=*/false);
      });
  renderer.ExecuteRenderGraph(graph);

  return buttons_image;
}

}  // namespace lighter::ui
