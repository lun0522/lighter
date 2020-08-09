//
//  util.cc
//
//  Created by Pujun Lun on 6/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/application/vulkan/util.h"

ABSL_FLAG(bool, performance_mode, false,
          "Ignore VSync and present images to the screen as fast as possible");

namespace lighter {
namespace application {
namespace vulkan {
namespace {

using namespace renderer::vulkan;

} /* namespace */

void OnScreenRenderPassManager::RecreateRenderPass() {
  /* Depth stencil image */
  if (subpass_config_.use_depth_stencil()) {
    depth_stencil_image_ = MultisampleImage::CreateDepthStencilImage(
        window_context_.basic_context(), window_context_.frame_size(),
        window_context_.multisampling_mode());
#ifndef NDEBUG
    LOG_INFO << "Depth stencil image created";
#endif /* !NDEBUG */
  }

  if (render_pass_builder_ == nullptr) {
    CreateRenderPassBuilder();
  }

  render_pass_builder_->UpdateAttachmentImage(
      swapchain_image_info_.index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context_.swapchain_image(framebuffer_index);
      });
  if (depth_stencil_image_ != nullptr) {
    render_pass_builder_->UpdateAttachmentImage(
        depth_stencil_image_info_.index(),
        [this](int framebuffer_index) -> const Image& {
          return *depth_stencil_image_;
        });
  }
  if (window_context_.use_multisampling()) {
    render_pass_builder_->UpdateAttachmentImage(
        multisample_image_info_.index(),
        [this](int framebuffer_index) -> const Image& {
          return window_context_.multisample_image();
        });
  }
  render_pass_ = render_pass_builder_->Build();
}

void OnScreenRenderPassManager::CreateRenderPassBuilder() {
  /* Image usage tracker */
  const bool use_depth_stencil = depth_stencil_image_ != nullptr;
  const bool use_multisampling = window_context_.use_multisampling();
  image::UsageTracker image_usage_tracker;
  swapchain_image_info_.AddToTracker(
      image_usage_tracker, window_context_.swapchain_image(/*index=*/0));
  if (use_depth_stencil) {
    depth_stencil_image_info_.AddToTracker(image_usage_tracker,
                                           *depth_stencil_image_);
  }
  if (use_multisampling) {
    multisample_image_info_.AddToTracker(image_usage_tracker,
                                         window_context_.multisample_image());
  }

  /* Render pass builder */
  const auto color_attachment_config =
      swapchain_image_info_.MakeAttachmentConfig()
          .set_final_usage(image::Usage::GetPresentationUsage());
  const auto multisampling_attachment_config =
      multisample_image_info_.MakeAttachmentConfig();
  const auto depth_stencil_attachment_config =
      depth_stencil_image_info_.MakeAttachmentConfig();
  render_pass_builder_ = NaiveRenderPass::CreateBuilder(
      window_context_.basic_context(),
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      subpass_config_, color_attachment_config,
      use_multisampling ? &multisampling_attachment_config : nullptr,
      use_depth_stencil ? &depth_stencil_attachment_config : nullptr,
      image_usage_tracker);
}

} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */
