//
//  post_effect.cc
//
//  Created by Pujun Lun on 4/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include <memory>
#include <string>

#include "lighter/application/vulkan/image_viewer.h"
#include "lighter/application/vulkan/util.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace {

using namespace renderer;
using namespace renderer::vulkan;

enum ComputeSubpassIndex {
  kPostEffectSubpassIndex = 0,
  kNumComputeSubpasses,
};

enum GraphicsSubpassIndex {
  kViewImageSubpassIndex = 0,
  kNumGraphicsSubpasses,
};

enum UniformBindingPoint {
  kOriginalImageBindingPoint = 0,
  kProcessedImageBindingPoint,
};

/* BEGIN: Consistent with work group size defined in shaders. */

constexpr uint32_t kWorkGroupSizeX = 32;
constexpr uint32_t kWorkGroupSizeY = 32;

/* END: Consistent with work group size defined in shaders. */

constexpr int kNumFramesInFlight = 2;

class PostEffectApp : public Application {
 public:
  explicit PostEffectApp(const WindowContext::Config& config);

  // This class is neither copyable nor movable.
  PostEffectApp(const PostEffectApp&) = delete;
  PostEffectApp& operator=(const PostEffectApp&) = delete;

  // Overrides.
  void MainLoop() override;

 private:
  // Populates 'processed_image_' and 'image_viewer_'.
  void ProcessImageFromFile(const std::string& file_path);

  // Recreates the swapchain and associated resources.
  void Recreate();

  // Accessors.
  const RenderPass& render_pass() const {
    return render_pass_manager_->render_pass();
  }

  int current_frame_ = 0;
  std::unique_ptr<OffscreenImage> processed_image_;
  std::unique_ptr<ImageViewer> image_viewer_;
  std::unique_ptr<PerFrameCommand> command_;
  std::unique_ptr<OnScreenRenderPassManager> render_pass_manager_;
};

} /* namespace */

PostEffectApp::PostEffectApp(const WindowContext::Config& window_config)
    : Application{"Post effect", window_config} {
  // No need to do multisampling.
  ASSERT_FALSE(window_context().use_multisampling(), "Not needed");

  /* Image and viewer */
  ProcessImageFromFile(common::file::GetResourcePath("texture/statue.jpg"));

  /* Command buffer */
  command_ = std::make_unique<PerFrameCommand>(context(), kNumFramesInFlight);

  /* Render pass */
  render_pass_manager_ = std::make_unique<OnScreenRenderPassManager>(
      &window_context(),
      NaiveRenderPass::SubpassConfig{
          kNumGraphicsSubpasses, /*first_transparent_subpass=*/std::nullopt,
          /*first_overlay_subpass=*/kViewImageSubpassIndex});
}

void PostEffectApp::ProcessImageFromFile(const std::string& file_path) {
  ImageUsageHistory original_image_usage_history{};
  original_image_usage_history.AddUsage(
      kPostEffectSubpassIndex,
      ImageUsage::GetLinearAccessInComputeShaderUsage(
          ir::AccessType::kReadOnly));
  const auto image_from_file =
      common::Image::LoadSingleImageFromFile(file_path, /*flip_y=*/false);
  TextureImage original_image(
      context(), /*generate_mipmaps=*/false, image_from_file,
      original_image_usage_history.GetAllUsages(), ImageSampler::Config{});

  ImageUsageHistory processed_image_usage_history{};
  processed_image_usage_history
      .AddUsage(kPostEffectSubpassIndex,
                ImageUsage::GetLinearAccessInComputeShaderUsage(
                    ir::AccessType::kWriteOnly))
      .SetFinalUsage(ImageUsage::GetSampledInFragmentShaderUsage());
  processed_image_ = std::make_unique<OffscreenImage>(
      context(), original_image.extent(), image_from_file.channel(),
      processed_image_usage_history.GetAllUsages(), ImageSampler::Config{},
      /*use_high_precision=*/false);

  const std::string original_image_name = "Original";
  const std::string processed_image_name = "Processed";
  ComputePass compute_pass{kNumComputeSubpasses};
  compute_pass
      .AddImage(original_image_name, std::move(original_image_usage_history))
      .AddImage(processed_image_name, std::move(processed_image_usage_history));

  StaticDescriptor descriptor{
      context(), /*infos=*/{
          Descriptor::Info{
              Image::GetDescriptorTypeForLinearAccess(),
              VK_SHADER_STAGE_COMPUTE_BIT,
              /*bindings=*/{
                  {kOriginalImageBindingPoint, /*array_length=*/1},
                  {kProcessedImageBindingPoint, /*array_length=*/1},
              },
          },
      }
  };
  const auto original_image_descriptor_info = original_image.GetDescriptorInfo(
      compute_pass.GetImageLayoutAtSubpass(original_image_name,
                                           kPostEffectSubpassIndex));
  const auto processed_image_descriptor_info =
      processed_image_->GetDescriptorInfo(
          compute_pass.GetImageLayoutAtSubpass(processed_image_name,
                                               kPostEffectSubpassIndex));
  descriptor.UpdateImageInfos(
      Image::GetDescriptorTypeForLinearAccess(),
      /*image_info_map=*/
      {{kOriginalImageBindingPoint, {original_image_descriptor_info}},
       {kProcessedImageBindingPoint, {processed_image_descriptor_info}}});

  const auto pipeline = ComputePipelineBuilder{context()}
      .SetPipelineName("Post effect")
      .SetPipelineLayout({descriptor.layout()}, /*push_constant_ranges=*/{})
      .SetShader(GetShaderBinaryPath("post_effect/sine_wave.comp"))
      .Build();

  const OneTimeCommand command{context(), &context()->queues().compute_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    const ComputePass::ComputeOp compute_op = [&]() {
      pipeline->Bind(command_buffer);
      descriptor.Bind(command_buffer, pipeline->layout(),
                      pipeline->binding_point());
      const auto group_count_x = renderer::vulkan::util::GetWorkGroupCount(
          image_from_file.width(), kWorkGroupSizeX);
      const auto group_count_y = renderer::vulkan::util::GetWorkGroupCount(
          image_from_file.height(), kWorkGroupSizeY);
      vkCmdDispatch(command_buffer, group_count_x, group_count_y,
                    /*groupCountZ=*/1);
    };
    compute_pass.Run(
        command_buffer, context()->queues().compute_queue().family_index,
        /*image_map=*/{
            {original_image_name, &original_image},
            {processed_image_name, processed_image_.get()},
        },
        absl::MakeSpan(&compute_op, 1));
  });

  image_viewer_ = std::make_unique<ImageViewer>(
      context(), *processed_image_, image_from_file.channel(), /*flip_y=*/true);
}

void PostEffectApp::Recreate() {
  render_pass_manager_->RecreateRenderPass();
  image_viewer_->UpdateFramebuffer(window_context().frame_size(), render_pass(),
                                   kViewImageSubpassIndex);
}

void PostEffectApp::MainLoop() {
  const RenderPass::RenderOp render_op =
      [this](const VkCommandBuffer& command_buffer) {
        image_viewer_->Draw(command_buffer);
      };

  Recreate();
  while (mutable_window_context()->CheckEvents()) {
    const auto draw_result = command_->Run(
        current_frame_, window_context().swapchain(), /*update_data=*/nullptr,
        [this, &render_op](const VkCommandBuffer& command_buffer,
                            uint32_t framebuffer_index) {
          render_pass().Run(command_buffer, framebuffer_index,
                            absl::MakeSpan(&render_op, 1));
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
  return AppMain<PostEffectApp>(
      argc, argv, WindowContext::Config{}.disable_multisampling());
}
