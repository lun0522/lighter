//
//  framebuffer.cc
//
//  Created by Pujun Lun on 10/2/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/framebuffer.h"

#include <optional>

#include "lighter/renderer/vk/image.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer::vk {
namespace {

using ir::RenderPassDescriptor;

// Extracts all color and depth stencil attachments.
std::vector<const Image*> ExtractAttachments(
    const RenderPassDescriptor& descriptor) {
  std::vector<const Image*> attachments;
  attachments.reserve(descriptor.color_ops_map.size() +
                      descriptor.depth_stencil_ops_map.size());
  for (const auto& [attachment, _]: descriptor.color_ops_map) {
    attachments.push_back(&Image::Cast(*attachment));
  }
  for (const auto& [attachment, _]: descriptor.depth_stencil_ops_map) {
    attachments.push_back(&Image::Cast(*attachment));
  }
  return attachments;
}

// Returns the number of framebuffers to create. This assumes that all instances
// of MultiImage have the same image count.
int FindNumFramebuffers(absl::Span<const Image* const> attachments) {
  std::optional<int> num_framebuffers;
  for (const Image* attachment : attachments) {
    if (attachment->IsSingleImage()) {
      continue;
    }
    const int num_images = MultiImage::Cast(*attachment).num_images();
    ASSERT_TRUE(
        !num_framebuffers.has_value() || num_images == num_framebuffers.value(),
        absl::StrFormat("Number of images (%d) in '%s' mismatches with other "
                        "attachments (found %d)",
                        num_images, attachment->name(),
                        num_framebuffers.value()));
    num_framebuffers = num_images;
  }
  return num_framebuffers.has_value() ? num_framebuffers.value() : 1;
}

class FramebuffersBuilder : public WithSharedContext {
 public:
  FramebuffersBuilder(
      const SharedContext& context, intl::RenderPass render_pass,
      const RenderPassDescriptor& descriptor)
      : WithSharedContext{context},
        attachments_{ExtractAttachments(descriptor)},
        num_framebuffers_{FindNumFramebuffers(attachments_)} {
    CreateImageViews();
    CreateFrameBuffers(render_pass);
  }
  
  // This class is neither copyable nor movable.
  FramebuffersBuilder(const FramebuffersBuilder&) = delete;
  FramebuffersBuilder& operator=(const FramebuffersBuilder&) = delete;

  // Move constructed objects out of the builder. These should be called only
  // once, and the builder should be discarded afterwards.
  std::vector<intl::ImageView> AcquireImageViews() {
    return std::move(image_views_);
  }
  std::vector<intl::Framebuffer> AcquireFramebuffers() {
    return std::move(framebuffers_);
  }

 private:
  // Populates `image_views_`.
  void CreateImageViews();

  // Create image views for `attachment` and appends to `image_views_`.
  void CreateAndAppendImageViews(const Image& attachment);

  // Populates `framebuffers_`.
  void CreateFrameBuffers(intl::RenderPass render_pass);

  const std::vector<const Image*> attachments_;
  const int num_framebuffers_;
  std::vector<intl::ImageView> image_views_;
  std::vector<intl::Framebuffer> framebuffers_;
};

void FramebuffersBuilder::CreateImageViews() {
  const int num_single_images = std::count_if(
      attachments_.begin(), attachments_.end(),
      [](const Image* image) { return image->IsSingleImage(); });
  const int num_multi_images = attachments_.size() - num_single_images;
  const int num_image_views =
      num_single_images + num_multi_images * num_framebuffers_;
  image_views_.reserve(num_image_views);
  for (const Image* attachment: attachments_) {
    CreateAndAppendImageViews(*attachment);
  }
}

void FramebuffersBuilder::CreateAndAppendImageViews(const Image& attachment) {
  const auto subresource_range = intl::ImageSubresourceRange{}
      .setAspectMask(attachment.GetAspectFlags())
      .setLevelCount(CAST_TO_UINT(attachment.mip_levels()))
      .setLayerCount(CAST_TO_UINT(attachment.GetNumLayers()));
  auto image_view_create_info = intl::ImageViewCreateInfo{}
      .setViewType(attachment.GetViewType())
      .setFormat(attachment.format())
      .setSubresourceRange(subresource_range);

  switch (attachment.type()) {
    case Image::Type::kSingle: {
      image_view_create_info.setImage(*SingleImage::Cast(attachment));
      image_views_.push_back(vk_device().createImageView(
          image_view_create_info, vk_host_allocator()));
      break;
    }

    case Image::Type::kMultiple: {
      const auto& multi_image = MultiImage::Cast(attachment);
      for (int i = 0; i < multi_image.num_images(); ++i) {
        image_view_create_info.setImage(multi_image.image(i));
        image_views_.push_back(vk_device().createImageView(
            image_view_create_info, vk_host_allocator()));
      }
      break;
    }
  }
}

void FramebuffersBuilder::CreateFrameBuffers(intl::RenderPass render_pass) {
  std::vector<intl::ImageView> image_views_for_framebuffer(attachments_.size());
  const Image& sample_attachemnt = *attachments_.front();
  const auto framebuffer_create_info = intl::FramebufferCreateInfo{}
      .setRenderPass(render_pass)
      .setAttachments(image_views_for_framebuffer)
      .setWidth(CAST_TO_UINT(sample_attachemnt.width()))
      .setHeight(CAST_TO_UINT(sample_attachemnt.height()))
      .setLayers(CAST_TO_UINT(sample_attachemnt.GetNumLayers()));

  framebuffers_.resize(num_framebuffers_);
  for (int framebuffer_index = 0; framebuffer_index < num_framebuffers_;
       ++framebuffer_index) {
    const intl::ImageView* image_view_ptr = image_views_.data();
    for (int attachment_index = 0; attachment_index < attachments_.size();
         ++attachment_index) {
      switch (attachments_[attachment_index]->type()) {
        case Image::Type::kSingle: {
          image_views_for_framebuffer[attachment_index] = *image_view_ptr;
          ++image_view_ptr;
          break;
        }
        
        case Image::Type::kMultiple: {
          image_views_for_framebuffer[attachment_index] =
              image_view_ptr[framebuffer_index];
          image_view_ptr += num_framebuffers_;
          break;
        }
      }
    }
    framebuffers_[framebuffer_index] = vk_device().createFramebuffer(
        framebuffer_create_info, vk_host_allocator());
  }
}

}  // namespace

Framebuffers::Framebuffers(
    const SharedContext& context, intl::RenderPass render_pass,
    const RenderPassDescriptor& descriptor)
    : WithSharedContext{context} {
  FramebuffersBuilder builder{context_, render_pass, descriptor};
  image_views_ = builder.AcquireImageViews();
  framebuffers_ = builder.AcquireFramebuffers();
}

Framebuffers::~Framebuffers() {
  for (intl::Framebuffer framebuffer : framebuffers_) {
    context_->DeviceDestroy(framebuffer);
  }
  for (intl::ImageView image_view : image_views_) {
    context_->DeviceDestroy(image_view);
  }
}

}  // namespace lighter::renderer::vk
