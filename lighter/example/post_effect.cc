//
//  post_effect.cc
//
//  Created by Pujun Lun on 4/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <cstdint>
#include <memory>

#include "lighter/common/image.h"
#include "lighter/example/util.h"

namespace lighter::example {
namespace {

using common::Vertex2D;
using renderer::ir::AccessType;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::ComputePipelineDescriptor;
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
using renderer::ir::TransientImageDesc;
using renderer::ir::TransientImageFormat;
using renderer::ir::VertexInputRate;

constexpr int kWindowIndex = 0;

// Consistent with work group size defined in post_effect/sine_wave.comp.
constexpr uint32_t kWorkGroupSizeX = 32;
constexpr uint32_t kWorkGroupSizeY = 32;

uint32_t GetWorkGroupCount(int extent, uint32_t work_group_size) {
  return (static_cast<uint32_t>(extent) + work_group_size - 1) /
         work_group_size;
}

class PostEffectExample {
 public:
  PostEffectExample(const glm::ivec2& screen_size,
                    MultisamplingMode multisampling_mode)
      : multisampling_mode_{multisampling_mode},
        window_{"Post effect", screen_size} {
    renderer_ = CreateRenderer("Post Effect Example", {&window_});

    const common::Image image_from_file =
        common::Image::LoadSingleImageFromFile(
            common::file::GetResourcePath("texture/statue.jpg"),
            /*flip_y=*/false);
    image_extent_ =
        glm::ivec2{image_from_file.width(), image_from_file.height()};
    original_image_ = renderer_->CreateColorImage(
        "OriginalStatue", image_from_file, /*generate_mipmaps=*/false,
        std::array{ImageUsage::GetSampledInFragmentShaderUsage()});

    const std::array<Vertex2D, 6> quad_vertices =
        Vertex2D::GetFullScreenSquadVertices(/*flip_y=*/true);
    const size_t quad_bytes = common::util::GetTotalDataSize(
        std::span<const Vertex2D>{quad_vertices});
    quad_vertex_buffer_ =
        renderer_->CreateBuffer(Buffer::UpdateRate::kLow, quad_bytes,
                                std::array{BufferUsage::GetVertexBufferUsage(
                                    BufferUsage::UsageType::kVertexOnly)});
    quad_vertex_buffer_->CopyToDevice(std::array{
        Buffer::CopyInfo{quad_vertices.data(), quad_bytes, /*offset=*/0}});

    viewer_sampler_ = {
        .filter_type = FilterType::kLinear,
        .address_mode = SamplerAddressMode::kClampToEdge,
    };

    const char* frag_shader =
        image_from_file.channel() == common::image::kBwImageChannel
            ? "image_viewer/view_bw_image.frag"
            : "image_viewer/view_color_image.frag";
    base_quad_pipeline_ =
        GraphicsPipelineDescriptor{}
            .SetName("ViewImage")
            .SetShader(renderer::ir::shader_stage::VERTEX,
                       GetShaderBinaryPath("image_viewer/view_image.vert"))
            .SetShader(renderer::ir::shader_stage::FRAGMENT,
                       GetShaderBinaryPath(frag_shader))
            .UseColorAttachment(/*location=*/0, std::nullopt)
            .AddVertexInput({
                VertexInputRate::kVertex,
                /*binding_point=*/0,
                /*stride=*/sizeof(Vertex2D),
                renderer::ir::buffer::CreateAttributesForVertex2D(
                    /*loc_pos=*/0, /*loc_tex_coord=*/1),
            });

    compute_pipeline_ =
        ComputePipelineDescriptor{}
            .SetName("SineWave")
            .SetShader(GetShaderBinaryPath("post_effect/sine_wave.comp"));
  }

  // This class is neither copyable nor movable.
  PostEffectExample(const PostEffectExample&) = delete;
  PostEffectExample& operator=(const PostEffectExample&) = delete;

  void MainLoop() {
    const uint32_t group_count_x =
        GetWorkGroupCount(image_extent_.x, kWorkGroupSizeX);
    const uint32_t group_count_y =
        GetWorkGroupCount(image_extent_.y, kWorkGroupSizeY);

    RenderGraph graph;
    while (!window_.ShouldQuit()) {
      window_.ProcessUserInputs();

      graph.Reset();
      const ImageHandle backbuffer = graph.ImportSwapchainImage(kWindowIndex);

      // Pass 1 (Graphics): Render original texture into a transient image.
      ImageHandle scene_color;
      graph.AddGraphicsPass(
          "CopyToScene",
          [&](PassBuilder& builder) {
            scene_color = builder.CreateImage(TransientImageDesc{
                .name = "scene_color",
                .extent = image_extent_,
                .format = TransientImageFormat::kColorUnorm8,
            });
            scene_color = builder.ColorAttachment(scene_color, /*location=*/0);
          },
          [&](PassContext& ctx) {
            ctx.BindGraphicsPipeline(BuildQuadPipeline(image_extent_));
            ctx.BindVertexBuffer(/*binding_point=*/0, *quad_vertex_buffer_);
            ctx.BindExternalImage(/*binding=*/0, *original_image_,
                                  &viewer_sampler_);
            ctx.Draw(/*vertex_count=*/6);
          });

      // Pass 2 (Compute): Apply sine-wave modulation via storage image access.
      ImageHandle processed_color;
      graph.AddComputePass(
          "SineWavePostEffect",
          [&](PassBuilder& builder) {
            scene_color = builder.Read(
                scene_color, ImageUsage::GetLinearAccessInComputeShaderUsage(
                                 AccessType::kReadOnly));
            processed_color = builder.CreateImage(TransientImageDesc{
                .name = "processed_color",
                .extent = image_extent_,
                .format = TransientImageFormat::kColorUnorm8,
            });
            processed_color =
                builder.Write(processed_color,
                              ImageUsage::GetLinearAccessInComputeShaderUsage(
                                  AccessType::kWriteOnly));
          },
          [&](PassContext& ctx) {
            ctx.BindComputePipeline(compute_pipeline_);
            ctx.BindImage(/*binding=*/0, scene_color);
            ctx.BindImage(/*binding=*/1, processed_color);
            ctx.Dispatch(group_count_x, group_count_y);
          });

      // Pass 3 (Graphics): Sample compute output onto the swapchain.
      graph.AddGraphicsPass(
          "PresentProcessedImage",
          [&](PassBuilder& builder) {
            processed_color = builder.Read(
                processed_color, ImageUsage::GetSampledInFragmentShaderUsage());
            SetupMainPassAttachments(builder, backbuffer, multisampling_mode_,
                                     /*use_depth=*/false);
          },
          [&](PassContext& ctx) {
            const glm::ivec2 frame_size = ctx.GetImageExtent(backbuffer);
            ctx.BindGraphicsPipeline(BuildQuadPipeline(frame_size));
            ctx.BindVertexBuffer(/*binding_point=*/0, *quad_vertex_buffer_);
            ctx.BindImage(/*binding=*/0, processed_color, &viewer_sampler_);
            ctx.Draw(/*vertex_count=*/6);
          });

      renderer_->RenderFrame(kWindowIndex, graph);
    }
    renderer_->WaitIdle();
  }

 private:
  GraphicsPipelineDescriptor BuildQuadPipeline(const glm::ivec2& extent) const {
    GraphicsPipelineDescriptor desc = base_quad_pipeline_;
    desc.SetViewport(renderer::ir::pipeline::GetFullFrameViewport(extent),
                     renderer::ir::pipeline::GetFullFrameScissor(extent),
                     /*flip_y=*/true);
    return desc;
  }

  const MultisamplingMode multisampling_mode_;
  glm::ivec2 image_extent_{0, 0};
  common::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<Image> original_image_;
  std::unique_ptr<Buffer> quad_vertex_buffer_;
  SamplerDescriptor viewer_sampler_{};
  GraphicsPipelineDescriptor base_quad_pipeline_;
  ComputePipelineDescriptor compute_pipeline_;
};

}  // namespace
}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::example::ExampleMain<lighter::example::PostEffectExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kNone);
}
