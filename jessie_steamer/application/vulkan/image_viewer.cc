//
//  image_viewer.cc
//
//  Created by Pujun Lun on 4/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <vector>

#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

constexpr int kNumFramesInFlight = 2;
constexpr uint32_t kVertexBufferBindingPoint = 0;
constexpr uint32_t kImageBindingPoint = 0;

enum SubpassIndex {
  kViewImageSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kViewImageSubpassIndex,
};

class ImageViewerApp : public Application {
 public:
  explicit ImageViewerApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Recreates the swapchain and associated resources.
  void Recreate();

  int current_frame_ = 0;
  std::unique_ptr<TextureImage> image_;
  std::unique_ptr<StaticDescriptor> descriptor_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
  std::unique_ptr<PipelineBuilder> pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace */

ImageViewerApp::ImageViewerApp(const WindowContext::Config& window_config)
    : Application{"Image viewer", window_config} {
  using common::Vertex2D;

  // No need to do multisampling.
  ASSERT_FALSE(window_context().use_multisampling(), "Not needed");

  /* Image */
  const common::Image image_to_view{
      common::file::GetResourcePath("texture/statue.jpg")};
  image_ = absl::make_unique<TextureImage>(
      context(), /*generate_mipmaps=*/false, SamplableImage::Config{},
      image_to_view);

  /* Descriptor */
  descriptor_ = absl::make_unique<StaticDescriptor>(
      context(), /*infos=*/std::vector<Descriptor::Info>{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              VK_SHADER_STAGE_FRAGMENT_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kImageBindingPoint,
                      /*array_length=*/1,
                  }},
          }});
  descriptor_->UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      /*image_info_map=*/{{kImageBindingPoint, {image_->GetDescriptorInfo()}}});

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Vertex buffer */
  const std::array<Vertex2D, 6> vertex_data{
      Vertex2D{/*pos=*/{-1.0f, -1.0f}, /*tex_coord=*/{0.0f, 0.0f}},
      Vertex2D{/*pos=*/{ 1.0f, -1.0f}, /*tex_coord=*/{1.0f, 0.0f}},
      Vertex2D{/*pos=*/{ 1.0f,  1.0f}, /*tex_coord=*/{1.0f, 1.0f}},
      Vertex2D{/*pos=*/{-1.0f, -1.0f}, /*tex_coord=*/{0.0f, 0.0f}},
      Vertex2D{/*pos=*/{ 1.0f,  1.0f}, /*tex_coord=*/{1.0f, 1.0f}},
      Vertex2D{/*pos=*/{-1.0f,  1.0f}, /*tex_coord=*/{0.0f, 1.0f}},
  };
  const PerVertexBuffer::NoIndicesDataInfo vertex_data_info{
      /*per_mesh_vertices=*/{{PerVertexBuffer::VertexDataInfo{vertex_data}}}
  };
  vertex_buffer_ = absl::make_unique<StaticPerVertexBuffer>(
      context(), vertex_data_info,
      pipeline::GetVertexAttribute<Vertex2D>());

  /* Render pass */
  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      context(), subpass_config,
      /*num_framebuffers=*/window_context().num_swapchain_images(),
      /*use_multisampling=*/false,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kPresentToScreen);

  /* Pipeline */
  const auto frag_shader_relative_path =
      image_to_view.channel == common::kBwImageChannel ?
          "view_bw_image.frag" : "view_color_image.frag";
  pipeline_builder_ = absl::make_unique<PipelineBuilder>(context());
  (*pipeline_builder_)
      .SetName("View image")
      .AddVertexInput(kVertexBufferBindingPoint,
                      pipeline::GetPerVertexBindingDescription<Vertex2D>(),
                      vertex_buffer_->GetAttributes(/*start_location=*/0))
      .SetPipelineLayout({descriptor_->layout()}, /*push_constant_ranges=*/{})
      .SetColorBlend({pipeline::GetColorBlendState(/*enable_blend=*/false)})
      .SetShader(VK_SHADER_STAGE_VERTEX_BIT,
                 common::file::GetVkShaderPath("view_image.vert"))
      .SetShader(VK_SHADER_STAGE_FRAGMENT_BIT,
                 common::file::GetVkShaderPath(frag_shader_relative_path));
}

void ImageViewerApp::Recreate() {
  /* Render pass */
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context().swapchain_image(framebuffer_index);
      });
  render_pass_ = (*render_pass_builder_)->Build();

  /* Pipeline */
  (*pipeline_builder_)
      .SetViewport(
          pipeline::GetFullFrameViewport(window_context().frame_size()))
      .SetRenderPass(**render_pass_, kViewImageSubpassIndex);
  pipeline_ = pipeline_builder_->Build();
}

void ImageViewerApp::MainLoop() {
  const std::vector<RenderPass::RenderOp> render_ops{
      [this](const VkCommandBuffer& command_buffer) {
        pipeline_->Bind(command_buffer);
        descriptor_->Bind(command_buffer, pipeline_->layout());
        vertex_buffer_->Draw(command_buffer, kVertexBufferBindingPoint,
                             /*mesh_index=*/0, /*instance_count=*/1);
      },
  };

  Recreate();
  while (mutable_window_context()->CheckEvents()) {
    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), /*update_data=*/nullptr,
        [this, &render_ops](const VkCommandBuffer& command_buffer,
                            uint32_t framebuffer_index) {
          render_pass_->Run(command_buffer, framebuffer_index, render_ops);
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
} /* namespace jessie_steamer */

int main(int argc, char* argv[]) {
  using namespace jessie_steamer::application::vulkan;
  return AppMain<ImageViewerApp>(argc, argv, WindowContext::Config{});
}
