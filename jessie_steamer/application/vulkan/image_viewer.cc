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
  std::unique_ptr<ImageViewer> image_viewer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace */

ImageViewerApp::ImageViewerApp(const WindowContext::Config& window_config)
    : Application{"Image viewer", window_config} {
  // No need to do multisampling.
  ASSERT_FALSE(window_context().use_multisampling(), "Not needed");

  /* Image and viewer */
  const common::Image image_to_view{
      common::file::GetResourcePath("texture/statue.jpg")};
  image_ = absl::make_unique<TextureImage>(
      context(), /*generate_mipmaps=*/false, SamplableImage::Config{},
      image_to_view);
  image_viewer_ = absl::make_unique<ImageViewer>(
      context(), *image_, image_to_view.channel);

  /* Command buffer */
  command_ = absl::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

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
}

void ImageViewerApp::Recreate() {
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context().swapchain_image(framebuffer_index);
      });
  render_pass_ = (*render_pass_builder_)->Build();
  image_viewer_->UpdateFramebuffer(window_context().frame_size(), *render_pass_,
                                   kViewImageSubpassIndex);
}

void ImageViewerApp::MainLoop() {
  const std::vector<RenderPass::RenderOp> render_ops{
      [this](const VkCommandBuffer& command_buffer) {
        image_viewer_->Draw(command_buffer);
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
