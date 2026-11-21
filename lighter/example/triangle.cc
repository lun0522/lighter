//
//  triangle.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>

#include "lighter/scene/util.h"

namespace lighter::example {
namespace {

using renderer::ir::AttachmentLoadOp;
using renderer::ir::AttachmentStoreOp;
using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::GraphicsPipelineDescriptor;
using renderer::ir::ImageHandle;
using renderer::ir::MultisamplingMode;
using renderer::ir::PassBuilder;
using renderer::ir::PassContext;
using renderer::ir::Renderer;
using renderer::ir::RenderGraph;
using renderer::ir::TransientImageDesc;
using renderer::ir::TransientImageFormat;
using renderer::ir::VertexInputRate;

constexpr int kWindowIndex = 0;

// BEGIN: Consistent with uniform blocks defined in shaders.

struct Alpha {
  ALIGN_SCALAR(float) float value;
};

// END: Consistent with uniform blocks defined in shaders.

}  // namespace

class TriangleExample {
 public:
  TriangleExample(const glm::ivec2& screen_size,
                  MultisamplingMode multisampling_mode)
      : window_{"Triangle", screen_size},
        multisampling_mode_{multisampling_mode} {
    using base::Vertex3DWithColor;

    renderer_ = scene::CreateRenderer("Triangle Example", {&window_});

    const std::array<Vertex3DWithColor, 3> vertex_data{
        Vertex3DWithColor{/*pos=*/{0.5f, -0.5f, 0.0f},
                          /*color=*/{1.0f, 0.0f, 0.0f}},
        Vertex3DWithColor{/*pos=*/{0.0f, 0.5f, 0.0f},
                          /*color=*/{0.0f, 0.0f, 1.0f}},
        Vertex3DWithColor{/*pos=*/{-0.5f, -0.5f, 0.0f},
                          /*color=*/{0.0f, 1.0f, 0.0f}},
    };
    const size_t vertex_data_size = base::util::GetTotalDataSize(
        std::span<const Vertex3DWithColor>{vertex_data});
    vertex_buffer_ =
        renderer_->CreateBuffer(Buffer::UpdateRate::kLow, vertex_data_size,
                                std::array{BufferUsage::GetVertexBufferUsage(
                                    BufferUsage::UsageType::kVertexOnly)});
    vertex_buffer_->CopyToDevice(
        std::array{Buffer::CopyInfo{vertex_data.data(), vertex_data_size,
                                    /*offset=*/0}});

    pipeline_descriptor_ =
        GraphicsPipelineDescriptor{}
            .SetName("Triangle")
            .SetShader(
                renderer::ir::shader_stage::VERTEX,
                base::file::GetShaderBinaryPath("triangle/triangle.vert"))
            .SetShader(
                renderer::ir::shader_stage::FRAGMENT,
                base::file::GetShaderBinaryPath("triangle/triangle.frag"))
            .UseColorAttachment(/*location=*/0,
                                renderer::ir::pipeline::GetColorAlphaBlend())
            .AddVertexInput(
                {VertexInputRate::kVertex,
                 /*binding_point=*/0,
                 /*stride=*/sizeof(Vertex3DWithColor),
                 renderer::ir::buffer::CreateAttributesForVertex3DWithColor(
                     /*loc_pos=*/0, /*loc_color=*/1)});
  }

  // This class is neither copyable nor movable.
  TriangleExample(const TriangleExample&) = delete;
  TriangleExample& operator=(const TriangleExample&) = delete;

  void MainLoop() {
    RenderGraph graph;

    while (!window_.ShouldQuit()) {
      window_.ProcessUserInputs();

      const Alpha alpha{glm::abs(glm::sin(timer_.GetElapsedTimeSinceLaunch()))};
      graph.Reset();
      const ImageHandle backbuffer = graph.ImportSwapchainImage(kWindowIndex);
      graph.AddGraphicsPass(
          "Triangle",
          [&](PassBuilder& builder) {
            scene::SetupMainPassAttachments(builder, backbuffer,
                                            multisampling_mode_,
                                            /*use_depth=*/false);
          },
          [&](PassContext& ctx) {
            ctx.BindGraphicsPipeline(pipeline_descriptor_);
            ctx.PushConstants(renderer::ir::shader_stage::FRAGMENT,
                              /*offset=*/0, sizeof(Alpha), &alpha);
            ctx.BindVertexBuffer(/*binding_point=*/0, *vertex_buffer_);
            ctx.Draw(/*vertex_count=*/3);
          });
      renderer_->RenderFrame(kWindowIndex, graph);
      timer_.Tick();
    }
    renderer_->WaitIdle();
  }

 private:
  const MultisamplingMode multisampling_mode_;
  base::FrameTimer timer_;
  base::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<Buffer> vertex_buffer_;
  GraphicsPipelineDescriptor pipeline_descriptor_;
};

}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::scene::RunApplication<lighter::example::TriangleExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kDecent);
}
