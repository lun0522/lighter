//
//  triangle.cc
//
//  Created by Pujun Lun on 10/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <memory>
#include <vector>

#include "lighter/application/vulkan/util.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace {

using namespace renderer::vulkan;

enum SubpassIndex {
  kRenderSubpassIndex = 0,
  kNumSubpasses,
};

constexpr int kNumFramesInFlight = 2;
constexpr uint32_t kVertexBufferBindingPoint = 0;

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Alpha {
  ALIGN_SCALAR(float) float value;
};

/* END: Consistent with uniform blocks defined in shaders. */

class TriangleApp : public Application {
 public:
  explicit TriangleApp(const WindowContext::Config& config);

  // This class is neither copyable nor movable.
  TriangleApp(const TriangleApp&) = delete;
  TriangleApp& operator=(const TriangleApp&) = delete;

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Updates per-frame data.
  void UpdateData(int frame);

  // Accessors.
  const RenderPass& render_pass() const {
    return render_pass_manager_->render_pass();
  }

  int current_frame_ = 0;
  common::FrameTimer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<PushConstant> alpha_constant_;
  std::unique_ptr<GraphicsPipelineBuilder> pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
  std::unique_ptr<OnScreenRenderPassManager> render_pass_manager_;
};

} /* namespace */

TriangleApp::TriangleApp(const WindowContext::Config& window_config)
    : Application{"Hello Triangle", window_config} {
  using common::Vertex3DWithColor;

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Vertex buffer */
  const std::array<Vertex3DWithColor, 3> vertex_data{
      Vertex3DWithColor{/*pos=*/{0.5f, -0.5f, 0.0f},
                        /*color=*/{1.0f, 0.0f, 0.0f}},
      Vertex3DWithColor{/*pos=*/{0.0f, 0.5f, 0.0f},
                        /*color=*/{0.0f, 0.0f, 1.0f}},
      Vertex3DWithColor{/*pos=*/{-0.5f, -0.5f, 0.0f},
                        /*color=*/{0.0f, 1.0f, 0.0f}},
  };
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context(), vertex_data_info,
      pipeline::GetVertexAttributes<Vertex3DWithColor>());

  /* Push constant */
  alpha_constant_ = absl::make_unique<PushConstant>(context(), sizeof(Alpha),
                                                    kNumFramesInFlight);

  /* Pipeline */
  pipeline_builder_ = absl::make_unique<GraphicsPipelineBuilder>(context());
  (*pipeline_builder_)
      .SetPipelineName("Triangle")
      .AddVertexInput(
          kVertexBufferBindingPoint,
          pipeline::GetPerVertexBindingDescription<Vertex3DWithColor>(),
          vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(
          /*descriptor_layouts=*/{},
          {alpha_constant_->MakePerFrameRange(VK_SHADER_STAGE_FRAGMENT_BIT)})
      .SetColorBlend({pipeline::GetColorAlphaBlendState(/*enable_blend=*/true)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("triangle/triangle.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("triangle/triangle.frag"));

  /* Render pass */
  render_pass_manager_ = absl::make_unique<OnScreenRenderPassManager>(
      &window_context(),
      NaiveRenderPass::SubpassConfig{
          kNumSubpasses, /*first_transparent_subpass=*/absl::nullopt,
          /*first_overlay_subpass=*/kRenderSubpassIndex});
}

void TriangleApp::Recreate() {
  /* Render pass */
  render_pass_manager_->RecreateRenderPass();

  /* Pipeline */
  (*pipeline_builder_)
      .SetMultisampling(window_context().sample_count())
      .SetViewport(
          pipeline::GetFullFrameViewport(window_context().frame_size()))
      .SetRenderPass(*render_pass(), kRenderSubpassIndex);
  pipeline_ = pipeline_builder_->Build();
}

void TriangleApp::UpdateData(int frame) {
  alpha_constant_->HostData<Alpha>(frame)->value =
      glm::abs(glm::sin(timer_.GetElapsedTimeSinceLaunch()));
}

void TriangleApp::MainLoop() {
  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (mutable_window_context()->CheckEvents()) {
    timer_.Tick();

    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), update_data,
        [this](const VkCommandBuffer& command_buffer,
               uint32_t framebuffer_index) {
          render_pass().Run(command_buffer, framebuffer_index, /*render_ops=*/{
              [this](const VkCommandBuffer& command_buffer) {
                pipeline_->Bind(command_buffer);
                alpha_constant_->Flush(command_buffer, pipeline_->layout(),
                                       current_frame_, /*target_offset=*/0,
                                       VK_SHADER_STAGE_FRAGMENT_BIT);
                vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                                     /*mesh_index=*/0, /*instance_count=*/1);
              },
          });
        });

    if (draw_result.has_value() || window_context().ShouldRecreate()) {
      mutable_window_context()->Recreate();
      Recreate();
    }
    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
  }
  mutable_window_context()->OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

int main(int argc, char* argv[]) {
  using namespace lighter::application::vulkan;
  return AppMain<TriangleApp>(argc, argv, WindowContext::Config{});
}
