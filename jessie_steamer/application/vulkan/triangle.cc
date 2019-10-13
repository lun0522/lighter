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

enum SubpassIndex {
  kTriangleSubpassIndex = 0,
  kNumSubpasses,
};

/* BEGIN: Consistent with structs used in shaders. */

struct Alpha {
  ALIGN_SCALAR(float) float value;
};

/* END: Consistent with structs used in shaders. */

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
  common::Timer timer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<Image> depth_stencil_image_;
  std::unique_ptr<PushConstant> push_constant_;
  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<PipelineBuilder> pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace */

TriangleApp::TriangleApp(const WindowContext::Config& window_config)
    : Application{"Hello Triangle", window_config} {
  using common::file::GetResourcePath;
  using common::file::GetShaderPath;
  using common::Vertex3DNoTex;

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Vertex buffer */
  std::array<Vertex3DNoTex, 3> vertex_data{
      Vertex3DNoTex{/*pos=*/{ 0.5f, -0.5f, 0.0f}, /*color=*/{1.0f, 0.0f, 0.0f}},
      Vertex3DNoTex{/*pos=*/{ 0.0f,  0.5f, 0.0f}, /*color=*/{0.0f, 0.0f, 1.0f}},
      Vertex3DNoTex{/*pos=*/{-0.5f, -0.5f, 0.0f}, /*color=*/{0.0f, 1.0f, 0.0f}},
  };
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_infos=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(context(),
                                                            vertex_data_info);

  /* Push constant */
  push_constant_ = absl::make_unique<PushConstant>(context(), sizeof(Alpha),
                                                   kNumFramesInFlight);
  const VkPushConstantRange push_constant_range{
      VK_SHADER_STAGE_FRAGMENT_BIT,
      /*offset=*/0,
      push_constant_->size_per_frame(),
  };

  /* Render pass */
  render_pass_builder_ = naive_render_pass::GetNaiveRenderPassBuilder(
      context(), kNumSubpasses,
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*present_to_screen=*/true, window_context_.multisampling_mode());

  /* Pipeline */
  const vector<pipeline::VertexInputBinding> bindings{
      pipeline::GetPerVertexBinding<Vertex3DNoTex>()};
  const vector<pipeline::VertexInputAttribute> attributes{
      pipeline::GetPerVertexAttribute<Vertex3DNoTex>()};
  pipeline_builder_ = absl::make_unique<PipelineBuilder>(context());
  (*pipeline_builder_)
      // TODO: No need to do depth test.
      .SetDepthTestEnabled(/*enable_test=*/true)
      .SetVertexInput(pipeline::GetBindingDescriptions(bindings),
                      pipeline::GetAttributeDescriptions(attributes))
      .SetPipelineLayout(/*descriptor_layouts=*/{}, {push_constant_range});
}

void TriangleApp::Recreate() {
  /* Depth image */
  const VkExtent2D& frame_size = window_context_.frame_size();
  depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
      context(), frame_size, window_context_.multisampling_mode());

  /* Render pass */
  if (window_context_.multisampling_mode().has_value()) {
    render_pass_builder_->UpdateAttachmentImage(
        naive_render_pass::kMultisampleAttachmentIndex,
        [this](int framebuffer_index) -> const Image& {
          return window_context_.multisample_image();
        });
  }
  render_pass_ = (*render_pass_builder_)
      .UpdateAttachmentImage(
          naive_render_pass::kColorAttachmentIndex,
          [this](int framebuffer_index) -> const Image& {
            return window_context_.swapchain_image(framebuffer_index);
          })
      .UpdateAttachmentImage(
          naive_render_pass::kDepthAttachmentIndex,
          [this](int framebuffer_index) -> const Image& {
            return *depth_stencil_image_;
          })
      .Build();

  /* Pipeline */
  using common::file::GetShaderPath;
  const auto sample_count = depth_stencil_image_->sample_count();
  (*pipeline_builder_)
      .SetMultisampling(sample_count)
      .SetViewport({
          /*viewport=*/VkViewport{
              /*x=*/0.0f,
              /*y=*/0.0f,
              static_cast<float>(frame_size.width),
              static_cast<float>(frame_size.height),
              /*minDepth=*/0.0f,
              /*maxDepth=*/1.0f,
          },
          /*scissor=*/VkRect2D{
              /*offset=*/{0, 0},
              frame_size,
          },
      })
      .SetRenderPass(**render_pass_, kTriangleSubpassIndex)
      .SetColorBlend(
          vector<VkPipelineColorBlendAttachmentState>(
              render_pass_->num_color_attachments(kTriangleSubpassIndex),
              pipeline::GetColorBlendState(/*enable_blend=*/true)))
      .AddShader({VK_SHADER_STAGE_VERTEX_BIT,
                  GetShaderPath("vulkan/simple_2d.vert.spv")})
      .AddShader({VK_SHADER_STAGE_FRAGMENT_BIT,
                  GetShaderPath("vulkan/simple_2d.frag.spv")});
  pipeline_ = pipeline_builder_->Build();
}

void TriangleApp::UpdateData(int frame) {
  const float elapsed_time = timer_.GetElapsedTimeSinceLaunch();
  push_constant_->HostData<Alpha>(frame)->value =
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
          push_constant_->Flush(command_buffer, pipeline_->layout(),
                                current_frame_, /*target_offset=*/0,
                                VK_SHADER_STAGE_FRAGMENT_BIT);
          vertex_buffer_->Draw(command_buffer, /*mesh_index=*/0,
                               /*instance_count=*/1);
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
  context()->WaitIdle();
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<TriangleApp>(argc, argv, WindowContext::Config{});
}
