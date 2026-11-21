//
//  renderer.cc
//
//  Created by Pujun Lun on 10/17/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/renderer.h"

namespace lighter::renderer::vk {

Renderer::Renderer(
    const char* application_name,
    const std::optional<ir::debug_message::Config>& debug_message_config,
    std::vector<const common::Window*>&& window_ptrs)
    : WithSharedContext{Context::CreateContext(
          application_name, debug_message_config, window_ptrs)},
      ir::Renderer{std::move(window_ptrs)} {
  swapchains_.resize(num_windows());
  for (int i = 0; i < num_windows(); ++i) {
    RecreateSwapchain(/*window_index=*/i);
  }
}

void Renderer::RecreateSwapchain(int window_index) {
  const auto& window = *windows().at(window_index);
  swapchains_[window_index] =
      std::make_unique<Swapchain>(context_, window_index, window);
}

std::unique_ptr<ir::RenderPass> Renderer::CreateRenderPass(
    ir::RenderPassDescriptor&& descriptor) const {
  class DynamicRenderPass : public ir::RenderPass {
   public:
    explicit DynamicRenderPass(const SharedContext& context,
                               const ir::RenderPassDescriptor& descriptor) {
      const ShaderModule::AutoReleaseShaderPool shader_pool;
      for (const auto& subpass : descriptor.subpass_descriptors) {
        Pipeline::RenderingInfo rendering_info;
        rendering_info.color_attachment_formats.reserve(
            subpass.color_attachments.size());
        intl::SampleCountFlagBits sample_count = intl::SampleCountFlagBits::e1;
        for (const ir::Image* attachment : subpass.color_attachments) {
          const auto& vk_image = Image::Cast(*attachment);
          rendering_info.color_attachment_formats.push_back(vk_image.format());
          sample_count = vk_image.sample_count();
        }
        if (subpass.depth_stencil_attachment != nullptr) {
          const auto& vk_ds = Image::Cast(*subpass.depth_stencil_attachment);
          rendering_info.depth_attachment_format = vk_ds.format();
          if (vk_ds.GetAspectFlags() & intl::ImageAspectFlagBits::eStencil) {
            rendering_info.stencil_attachment_format = vk_ds.format();
          }
          if (subpass.color_attachments.empty()) {
            sample_count = vk_ds.sample_count();
          }
        }
        for (const auto& pipeline_desc : subpass.pipeline_descriptors) {
          pipelines_.push_back(std::make_unique<Pipeline>(
              context, pipeline_desc, sample_count, rendering_info));
        }
      }
    }

   private:
    std::vector<std::unique_ptr<Pipeline>> pipelines_;
  };
  return std::make_unique<DynamicRenderPass>(context_, descriptor);
}

}  // namespace lighter::renderer::vk
