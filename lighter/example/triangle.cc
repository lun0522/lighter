//
//  triangle.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/util.h"

#include <array>

namespace lighter::example {
namespace {

using renderer::ir::Buffer;
using renderer::ir::BufferUsage;
using renderer::ir::GraphicsPipelineDescriptor;
using renderer::ir::Image;
using renderer::ir::ImageUsage;
using renderer::ir::MultisamplingMode;
using renderer::ir::RenderPass;
using renderer::ir::RenderPassDescriptor;
using renderer::ir::Renderer;
using renderer::ir::SubpassDescriptor;
using renderer::ir::VertexInputRate;

constexpr int kWindowIndex = 0;
constexpr int kNumFramesInFlight = 2;

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
      : window_{"Triangle", screen_size}, alpha_{kNumFramesInFlight} {
    using common::Vertex3DWithColor;

    const bool use_multisamping =
        multisampling_mode != MultisamplingMode::kNone;
    renderer_ = CreateRenderer("Triangle Example", {&window_});

    const std::array<Vertex3DWithColor, 3> vertex_data{
        Vertex3DWithColor{/*pos=*/{0.5f, -0.5f, 0.0f},
                          /*color=*/{1.0f, 0.0f, 0.0f}},
        Vertex3DWithColor{/*pos=*/{0.0f, 0.5f, 0.0f},
                          /*color=*/{0.0f, 0.0f, 1.0f}},
        Vertex3DWithColor{/*pos=*/{-0.5f, -0.5f, 0.0f},
                          /*color=*/{0.0f, 1.0f, 0.0f}},
    };
    const size_t vertex_data_size =
        common::util::GetTotalDataSize(std::span<const Vertex3DWithColor>{
            vertex_data});
    // TODO: change to UpdateRate::kLow.
    vertex_buffer_ = renderer_->CreateBuffer(
        Buffer::UpdateRate::kHigh, vertex_data_size,
        std::array{BufferUsage::GetVertexBufferUsage(
            BufferUsage::UsageType::kVertexOnly)});
    vertex_buffer_->CopyToDevice(
        std::array{Buffer::CopyInfo{vertex_data.data(), vertex_data_size,
                                    /*offset=*/0}});

    uniform_buffer_ = renderer_->CreateBuffer<Alpha>(
        Buffer::UpdateRate::kHigh, kNumFramesInFlight,
        std::array{BufferUsage::GetUniformBufferUsage(
            renderer::ir::AccessLocation::kFragmentShader)});

    // TODO: Use refection API for locations.
    auto pipeline_descriptor = GraphicsPipelineDescriptor{}
        .SetName("Triangle")
        .SetShader(renderer::ir::shader_stage::VERTEX,
                   GetShaderBinaryPath("triangle/triangle.vert"))
        .SetShader(renderer::ir::shader_stage::FRAGMENT,
                   GetShaderBinaryPath("triangle/triangle.frag"))
        .UseColorAttachment(/*location=*/0,
                            renderer::ir::pipeline::GetColorAlphaBlend())
        .AddVertexInput({
            VertexInputRate::kVertex,
            /*binding_point=*/0,
            /*stride=*/sizeof(vertex_data[0]),
            renderer::ir::buffer::CreateAttributesForVertex3DWithColor(
                /*loc_pos=*/0, /*loc_color=*/1)});

    const Image& swapchain_image = renderer_->GetSwapchainImage(kWindowIndex);
    auto subpass_descriptor = SubpassDescriptor{}
        .AddPipeline(std::move(pipeline_descriptor));
    auto render_pass_descriptor = RenderPassDescriptor{};

    if (use_multisamping) {
      multisample_attachment_ = renderer_->CreateColorImage(
          "color_multisample", {screen_size, common::image::kRgbaImageChannel},
          multisampling_mode, /*high_precision=*/false,
          std::array{ImageUsage::GetRenderTargetUsage(
              /*attachment_location=*/0)});
      subpass_descriptor
          .AddColorAttachment(multisample_attachment_.get())
          .AddMultisampleResolve(multisample_attachment_.get(),
                                 &swapchain_image);
      render_pass_descriptor
          .AddAttachment(
              multisample_attachment_.get(),
              renderer::ir::pass::GetRenderTargetLoadStoreOps(
                  /*is_multisampled=*/true))
          .AddAttachment(
              &swapchain_image,
              renderer::ir::pass::GetResolveTargetLoadStoreOps());
    } else {
      subpass_descriptor.AddColorAttachment(&swapchain_image);
      render_pass_descriptor
          .AddAttachment(
              &swapchain_image,
              renderer::ir::pass::GetRenderTargetLoadStoreOps(
                  /*is_multisampled=*/false));
    }

    render_pass_descriptor.AddSubpass(std::move(subpass_descriptor));
    render_pass_ =
        renderer_->CreateRenderPass(std::move(render_pass_descriptor));
  }

  // This class is neither copyable nor movable.
  TriangleExample(const TriangleExample&) = delete;
  TriangleExample& operator=(const TriangleExample&) = delete;

  void MainLoop() {
    while (!window_.ShouldQuit()) {
      window_.ProcessUserInputs();

      alpha_.GetMutData(current_frame_)->value =
          glm::abs(glm::sin(timer_.GetElapsedTimeSinceLaunch()));
      uniform_buffer_->CopyToDevice(
          std::array{Buffer::CopyInfo{alpha_.GetData(current_frame_),
                                      sizeof(Alpha),
                                      sizeof(Alpha) * current_frame_}});

      current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
      timer_.Tick();
    }
  }

 private:
  int current_frame_ = 0;
  common::FrameTimer timer_;
  common::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<Buffer> vertex_buffer_;
  common::TypedChunkedData<Alpha> alpha_;
  std::unique_ptr<Buffer> uniform_buffer_;
  std::unique_ptr<Image> multisample_attachment_;
  std::unique_ptr<RenderPass> render_pass_;
};

}  // namespace lighter::example

int main(int argc, char* argv[]) {
  return lighter::example::ExampleMain<lighter::example::TriangleExample>(
      argc, argv, glm::ivec2{800, 600},
      lighter::renderer::ir::MultisamplingMode::kDecent);
}
