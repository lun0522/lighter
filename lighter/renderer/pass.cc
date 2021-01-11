//
//  pass.cc
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/pass.h"

#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace {

std::vector<const DeviceImage*> ExtractImages(
    absl::Span<const GraphicsPassDescriptor::ColorAttachment> color_attachments,
    absl::Span<const GraphicsPassDescriptor::DepthStencilAttachment>
        depth_stencil_attachments,
    absl::Span<const DeviceImage* const> uniform_textures) {
  std::vector<const DeviceImage*> images{uniform_textures.begin(),
                                         uniform_textures.end()};
  images.reserve(images.size() + color_attachments.size() +
                 depth_stencil_attachments.size());
  for (const auto& attachment : color_attachments) {
    images.push_back(&attachment.image);
  }
  for (const auto& attachment : depth_stencil_attachments) {
    images.push_back(&attachment.image);
  }
  return images;
}

} /* namespace */

PassDescriptor::PassDescriptor(absl::Span<const DeviceImage* const> images) {
  for (const DeviceImage* image : images) {
    FATAL_IF_NULL(image);
    ASSERT_TRUE(image_usage_history_map_.find(image) ==
                    image_usage_history_map_.end(),
                absl::StrFormat("Duplicated image '%s'", image->name()));
    image_usage_history_map_.insert({image, {}});
  }
}

void PassDescriptor::AddSubpass(
    absl::Span<const ImageAndUsage> images_and_usages) {
  for (const ImageAndUsage& image_and_usage : images_and_usages) {
    auto iter = image_usage_history_map_.find(&image_and_usage.image);
    ASSERT_FALSE(iter == image_usage_history_map_.end(), "Unrecognized image");
    iter->second.insert({num_subpasses_, image_and_usage.usage});
  }
  ++num_subpasses_;
}

GraphicsPassDescriptor::GraphicsPassDescriptor(
    absl::Span<const ColorAttachment> color_attachments,
    absl::Span<const DepthStencilAttachment> depth_stencil_attachments,
    absl::Span<const DeviceImage* const> uniform_textures)
    : PassDescriptor{ExtractImages(color_attachments, depth_stencil_attachments,
                                   uniform_textures)} {
  for (const auto& attachment : color_attachments) {
    color_ops_map_.insert({&attachment.image, attachment.load_store_ops});
  }
  for (const auto& attachment : depth_stencil_attachments) {
    depth_stencil_ops_map_.insert({&attachment.image,
                                   attachment.load_store_ops});
  }
}

GraphicsPassDescriptor& GraphicsPassDescriptor::AddSubpass(
    absl::Span<const ImageAndUsage> images_and_usages,
    absl::Span<const MultisamplingResolve> multisampling_resolves,
    absl::Span<const GraphicsPipelineDescriptor> pipeline_descriptors) {
  PassDescriptor::AddSubpass(images_and_usages);
  for (const auto& resolve : multisampling_resolves) {
    for (const auto* image : {&resolve.source_image, &resolve.target_image}) {
      ASSERT_FALSE(image_usage_history_map().find(image) ==
                       image_usage_history_map().end(),
                   absl::StrFormat("Image '%s' is not a part of this pass",
                                   image->name()));
    }
  }
  multisampling_resolves_.emplace_back(multisampling_resolves.begin(),
                                       multisampling_resolves.end());
  return *this;
}

} /* namespace renderer */
} /* namespace lighter */
