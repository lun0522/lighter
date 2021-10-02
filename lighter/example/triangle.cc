//
//  triangle.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/util.h"

namespace lighter::example {
namespace {

constexpr int kWindowIndex = 0;

// BEGIN: Consistent with uniform blocks defined in shaders.

struct Alpha {
  ALIGN_SCALAR(float) float value;
};

// END: Consistent with uniform blocks defined in shaders.

}  // namespace

class TriangleExample {
 public:
  TriangleExample(common::api::GraphicsApi graphics_api,
                  const glm::ivec2& screen_size,
                  MultisamplingMode multisampling_mode)
      : window_{"Triangle", screen_size} {
    const bool use_multisamping =
        multisampling_mode != MultisamplingMode::kNone;
    renderer_ = CreateRenderer(graphics_api, "Triangle Example", {&window_});

    // TODO: Use refection API for locations.
    auto pipeline_descriptor = GraphicsPipelineDescriptor{}
        .SetName("Triangle")
        .SetShader(shader_stage::VERTEX,
                   GetShaderBinaryPath("triangle/triangle.vert", graphics_api))
        .SetShader(shader_stage::FRAGMENT,
                   GetShaderBinaryPath("triangle/triangle.frag", graphics_api))
        .UseColorAttachment(/*location=*/0, pipeline::GetColorAlphaBlend())
        .AddVertexInput({
            VertexInputRate::kVertex,
            /*binding_point=*/0,
            /*stride=*/sizeof(common::Vertex3DWithColor),
            buffer::CreateAttributesForVertex3DWithColor(/*loc_pos=*/0,
                                                         /*loc_color=*/1)})
        // TODO: Create helper function to make range.
        .AddPushConstantRange({shader_stage::FRAGMENT, /*offset=*/0,
                              sizeof(Alpha)});
    
    const Image& swapchain_image = renderer_->GetSwapchainImage(kWindowIndex);
    auto subpass_descriptor = SubpassDescriptor{}
        .AddPipeline(std::move(pipeline_descriptor));
    auto render_pass_descriptor = RenderPassDescriptor{};

    if (use_multisamping) {
      multisample_attachment_ = renderer_->CreateColorImage(
          "color_multisample", {screen_size, common::image::kRgbaImageChannel},
          multisampling_mode, /*high_precision=*/false,
          {ImageUsage::GetRenderTargetUsage(/*attachment_location=*/0)});
      subpass_descriptor
          .AddColorAttachment(multisample_attachment_.get())
          .AddMultisampleResolve(multisample_attachment_.get(),
                                 &swapchain_image);
      render_pass_descriptor
          .SetLoadStoreOps(
              multisample_attachment_.get(),
              pass::GetRenderTargetLoadStoreOps(/*is_multisampled=*/true))
          .SetLoadStoreOps(&swapchain_image,
                           pass::GetResolveTargetLoadStoreOps());
    } else {
      subpass_descriptor.AddColorAttachment(&swapchain_image);
      render_pass_descriptor
          .SetLoadStoreOps(
              &swapchain_image,
              pass::GetRenderTargetLoadStoreOps(/*is_multisampled=*/false));
    }

    render_pass_descriptor.AddSubpass(std::move(subpass_descriptor));
    render_pass_ =
        renderer_->CreateRenderPass(std::move(render_pass_descriptor));
  }

  // This class is neither copyable nor movable.
  TriangleExample(const TriangleExample&) = delete;
  TriangleExample& operator=(const TriangleExample&) = delete;

  void MainLoop() {}

 private:
  common::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<DeviceBuffer> vertex_buffer_;
  std::unique_ptr<Image> multisample_attachment_;
  std::unique_ptr<RenderPass> render_pass_;
};

}  // namespace lighter::example

int main(int argc, char* argv[]) {
  using namespace lighter::example;
  return ExampleMain<TriangleExample>(
      argc, argv, lighter::common::api::GraphicsApi::kVulkan,
      glm::ivec2{800, 600}, MultisamplingMode::kDecent);
}
