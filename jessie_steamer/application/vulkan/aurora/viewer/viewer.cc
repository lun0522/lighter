//
//  viewer.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/viewer.h"

#include "jessie_steamer/common/util.h"
#include "third_party/absl/types/span.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

enum SubpassIndex {
  kViewImageSubpassIndex = 0,
  kNumSubpasses,
  kNumOverlaySubpasses = kNumSubpasses - kViewImageSubpassIndex,
};

} /* namespace */

Viewer::Viewer(
    WindowContext* window_context, int num_frames_in_flight,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : window_context_{*FATAL_IF_NULL(window_context)},
      path_dumper_{window_context_.basic_context(),
                   /*paths_image_dimension=*/2000,
                   /*camera_field_of_view=*/30.0f,
                   std::move(aurora_paths_vertex_buffers)} {
  image_viewer_ = absl::make_unique<ImageViewer>(
      window_context_.basic_context(), path_dumper_.paths_image(),
      /*num_channels=*/1, /*flip_y=*/true);

  const NaiveRenderPassBuilder::SubpassConfig subpass_config{
      /*use_opaque_subpass=*/false,
      /*num_transparent_subpasses=*/0,
      kNumOverlaySubpasses,
  };
  render_pass_builder_ = absl::make_unique<NaiveRenderPassBuilder>(
      window_context_.basic_context(), subpass_config,
      /*num_framebuffers=*/window_context_.num_swapchain_images(),
      /*use_multisampling=*/false,
      NaiveRenderPassBuilder::ColorAttachmentFinalUsage::kPresentToScreen);
}

void Viewer::OnEnter() {
  // TODO: Find a better way to exit viewer.
  did_press_right_ = false;
  window_context_.mutable_window()->RegisterMouseButtonCallback(
      [this](bool is_left, bool is_press) {
        did_press_right_ = !is_left && is_press;
      });
}

void Viewer::OnExit() {
  window_context_.mutable_window()->RegisterMouseButtonCallback(nullptr);
}

void Viewer::Recreate() {
  render_pass_builder_->mutable_builder()->UpdateAttachmentImage(
      render_pass_builder_->color_attachment_index(),
      [this](int framebuffer_index) -> const Image& {
        return window_context_.swapchain_image(framebuffer_index);
      });
  render_pass_ = (*render_pass_builder_)->Build();
  image_viewer_->UpdateFramebuffer(window_context_.frame_size(), *render_pass_,
                                   kViewImageSubpassIndex);
}

void Viewer::Draw(const VkCommandBuffer& command_buffer,
                  uint32_t framebuffer_index, int current_frame) {
  render_pass_->Run(command_buffer, framebuffer_index, /*render_ops=*/{
        [this](const VkCommandBuffer& command_buffer) {
          image_viewer_->Draw(command_buffer);
        },
  });
}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
