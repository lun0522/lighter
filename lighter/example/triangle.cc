//
//  triangle.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/example/util.h"

namespace lighter::example {
namespace {

using namespace renderer;

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
    renderer_ = CreateRenderer(graphics_api, "Triangle Example", {&window_});

    // TODO: Use refection API for locations.
    pipeline_descriptor_
        .SetName("Triangle")
        .SetShader(shader_stage::VERTEX,
                   GetShaderBinaryPath("triangle/triangle.vert", graphics_api))
        .SetShader(shader_stage::FRAGMENT,
                   GetShaderBinaryPath("triangle/triangle.frag", graphics_api))
        .AddColorAttachment(&renderer_->GetSwapchainImage(kWindowIndex),
                            {/*location=*/0, pipeline::GetColorAlphaBlend()})
        .AddVertexInput({
            VertexInputRate::kVertex,
            /*binding_point=*/0,
            /*stride=*/sizeof(common::Vertex3DWithColor),
            buffer::CreateAttributesForVertex3DWithColor(/*loc_pos=*/0,
                                                         /*loc_color=*/1)})
        // TODO: Create helper function to make range.
        .AddPushConstantRange({shader_stage::FRAGMENT, /*offset=*/0,
                              sizeof(Alpha)});
  }

  // This class is neither copyable nor movable.
  TriangleExample(const TriangleExample&) = delete;
  TriangleExample& operator=(const TriangleExample&) = delete;

  void MainLoop() {}

 private:
  common::Window window_;
  std::unique_ptr<Renderer> renderer_;
  std::unique_ptr<DeviceBuffer> vertex_buffer_;
  GraphicsPipelineDescriptor pipeline_descriptor_;
};

}  // namespace lighter::example

int main(int argc, char* argv[]) {
  using namespace lighter::example;
  return ExampleMain<TriangleExample>(
      argc, argv, lighter::common::api::GraphicsApi::kVulkan,
      glm::ivec2{800, 600}, MultisamplingMode::kDecent);
}
