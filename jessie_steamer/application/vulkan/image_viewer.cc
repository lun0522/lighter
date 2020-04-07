//
//  image_viewer.cc
//
//  Created by Pujun Lun on 4/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <cmath>
#include <vector>

#include "jessie_steamer/application/vulkan/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace {

using namespace wrapper::vulkan;

enum UniformBindingPoint {
  kOriginalImageBindingPoint = 0,
  kOutputImageBindingPoint,
};

/* BEGIN: Consistent with work group size defined in shaders. */

constexpr uint32_t kWorkGroupSizeX = 32;
constexpr uint32_t kWorkGroupSizeY = 32;

/* END: Consistent with work group size defined in shaders. */

enum SubpassIndex {
  kViewImageSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kViewImageSubpassIndex,
};

constexpr int kNumFramesInFlight = 2;

class ImageViewerApp : public Application {
 public:
  explicit ImageViewerApp(const WindowContext::Config& config);

  // Overrides.
  void MainLoop() override;

 private:
  // Populates 'image_' and 'image_viewer_'.
  void ProcessImageFromFile(const std::string& file_path);

  // Recreates the swapchain and associated resources.
  void Recreate();

  int current_frame_ = 0;
  std::unique_ptr<OffscreenImage> image_;
  std::unique_ptr<ImageViewer> image_viewer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<RenderPass> render_pass_;
};

// Return the work group size for one dimension.
inline uint32_t GetWorkGroupCount(int image_size, uint32_t work_group_size) {
  return static_cast<uint32_t>(std::ceil(
      static_cast<float>(image_size) / work_group_size));
}

} /* namespace */

ImageViewerApp::ImageViewerApp(const WindowContext::Config& window_config)
    : Application{"Image viewer", window_config} {
  // No need to do multisampling.
  ASSERT_FALSE(window_context().use_multisampling(), "Not needed");

  /* Image and viewer */
  ProcessImageFromFile(common::file::GetResourcePath("texture/statue.jpg"));

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

void ImageViewerApp::ProcessImageFromFile(const std::string& file_path) {
  const common::Image image_from_file{file_path};
  const TextureImage original_image(context(), /*generate_mipmaps=*/false,
                                    image_from_file, SamplableImage::Config{});
  image_ = absl::make_unique<OffscreenImage>(
      context(), OffscreenImage::DataSource::kCompute, image_from_file.channel,
      original_image.extent(), SamplableImage::Config{});

  StaticDescriptor descriptor{
      context(), /*infos=*/std::vector<Descriptor::Info>{
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              VK_SHADER_STAGE_COMPUTE_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kOriginalImageBindingPoint,
                      /*array_length=*/1,
                  }},
          },
          Descriptor::Info{
              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              VK_SHADER_STAGE_COMPUTE_BIT,
              /*bindings=*/{
                  Descriptor::Info::Binding{
                      kOutputImageBindingPoint,
                      /*array_length=*/1,
                  }},
          },
      }
  };
  // TODO
  auto output_image_descriptor_info = image_->GetDescriptorInfo();
  output_image_descriptor_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  descriptor.UpdateImageInfos(
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
      /*image_info_map=*/
      {{kOriginalImageBindingPoint, {original_image.GetDescriptorInfo()}},
       {kOutputImageBindingPoint, {output_image_descriptor_info}}});

  const auto pipeline = ComputePipelineBuilder{context()}
      .SetPipelineName("Process image")
      .SetPipelineLayout({descriptor.layout()}, /*push_constant_ranges=*/{})
      .SetShader(
          common::file::GetVkShaderPath("image_viewer/process_image.comp"))
      .Build();

  const OneTimeCommand command{context(), &context()->queues().compute_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    pipeline->Bind(command_buffer);
    descriptor.Bind(command_buffer, pipeline->layout(),
                    pipeline->binding_point());
    const auto group_count_x = GetWorkGroupCount(image_from_file.width,
                                                 kWorkGroupSizeX);
    const auto group_count_y = GetWorkGroupCount(image_from_file.height,
                                                 kWorkGroupSizeY);
    vkCmdDispatch(command_buffer, group_count_x, group_count_y,
                  /*groupCountZ=*/1);
  });

  image_viewer_ = absl::make_unique<ImageViewer>(
      context(), *image_, image_from_file.channel, /*flip_y=*/true);
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
  return AppMain<ImageViewerApp>(
      argc, argv, WindowContext::Config{}.disable_multisampling());
}
