//
//  triangle.cc
//
//  Created by Pujun Lun on 10/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <array>
#include <memory>
#include <vector>

#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

using std::vector;

constexpr int kNumFramesInFlight = 2;
constexpr uint32_t kVertexBufferBindingPoint = 0;

enum SubpassIndex {
  kTriangleSubpassIndex = 0,
  kNumSubpasses,
};

/* BEGIN: Consistent with uniform blocks defined in shaders. */

struct Alpha {
  ALIGN_SCALAR(float) float value;
};

/* END: Consistent with uniform blocks defined in shaders. */

class TriangleApp : public Application {
 public:
  explicit TriangleApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  // Updates per-frame data.
  void UpdateData(int frame);

  int current_frame_ = 0;
  common::FrameTimer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<PushConstant> alpha_constant_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<PipelineBuilder> pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace */

TriangleApp::TriangleApp(const WindowContext::Config& window_config)
    : Application{"Hello Triangle", window_config} {
  using common::Vertex3DNoTex;

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Vertex buffer */
  const std::array<Vertex3DNoTex, 3> vertex_data{
      Vertex3DNoTex{/*pos=*/{ 0.5f, -0.5f, 0.0f}, /*color=*/{1.0f, 0.0f, 0.0f}},
      Vertex3DNoTex{/*pos=*/{ 0.0f,  0.5f, 0.0f}, /*color=*/{0.0f, 0.0f, 1.0f}},
      Vertex3DNoTex{/*pos=*/{-0.5f, -0.5f, 0.0f}, /*color=*/{0.0f, 1.0f, 0.0f}},
  };
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context(), vertex_data_info,
      pipeline::GetVertexAttribute<Vertex3DNoTex>());

  /* Push constant */
  alpha_constant_ = absl::make_unique<PushConstant>(context(), sizeof(Alpha),
                                                   kNumFramesInFlight);
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_FRAGMENT_BIT,
      /*offset=*/0,
      alpha_constant_->size_per_frame(),
  };

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      /*num_overlay_subpasses=*/kNumSubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context(), subpass_config,
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*present_to_screen=*/true, window_context_.multisampling_mode());

  /* Pipeline */
  pipeline_builder_ = absl::make_unique<PipelineBuilder>(context());
  (*pipeline_builder_)
      .SetName("triangle")
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex3DNoTex>(),
                      vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout(/*descriptor_layouts=*/{}, {push_constant_range})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("pure_color.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath("pure_color.frag"));
}

void TriangleApp::Recreate() {
  /* Render pass */
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context_.swapchain_image(framebuffer_index);
      });
  if (render_pass_builder_->has_multisample_attachment()) {
    render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
        render_pass_builder_->multisample_attachment_index(),
        [this](int framebuffer_index) -> const Image& {
          return window_context_.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)->Build();

  /* Pipeline */
  (*pipeline_builder_)
      .SetMultisampling(window_context_.sample_count())
      .SetViewport(pipeline::GetFullFrameViewport(window_context_.frame_size()))
      .SetRenderPass(**render_pass_, kTriangleSubpassIndex)
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/true)});
  pipeline_ = pipeline_builder_->Build();
}

void TriangleApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
  alpha_constant_->HostData<Alpha>(frame)->value =
      glm::abs(glm::sin(elapsed_time));
}

void TriangleApp::MainLoop() {
  const auto update_data = [this](int frame) { UpdateData(frame); };

  Recreate();
  while (window_context_.CheckEvents()) {
    timer_.Tick();

    const vector<RenderPass::RenderOp> render_ops{
        [&](const VkCommandBuffer& command_buffer) {
          pipeline_->Bind(command_buffer);
          alpha_constant_->Flush(command_buffer, pipeline_->layout(),
                                 current_frame_, /*target_offset=*/0,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
          vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                               /*mesh_index=*/0, /*instance_count=*/1);
        },
    };
    const auto draw_result = command_->Run(
        current_frame_, window_context_.swapchain(), update_data,
        [&](const VkCommandBuffer& command_buffer, uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
        });

    if (draw_result.has_value() || window_context_.ShouldRecreate()) {
      window_context_.Recreate();
      Recreate();
    }
    current_frame_ = (current_frame_ + 1) % kNumFramesInFlight;
  }
  window_context_.OnExit();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<TriangleApp>(argc, argv, WindowContext::Config{});
}
