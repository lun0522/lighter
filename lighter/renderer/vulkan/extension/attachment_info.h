//
//  attachment_info.h
//
//  Created by Pujun Lun on 7/26/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_ATTACHMENT_INFO_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_ATTACHMENT_INFO_H

#include <functional>
#include <string>

#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Holds identifiers of an attachment image and provides util functions for
// interacting with image::UsageTracker and GraphicsPass.
class AttachmentInfo {
 public:
  explicit AttachmentInfo(std::string&& name) : name_{std::move(name)} {}

  // This class provides copy constructor and move constructor.
  AttachmentInfo(AttachmentInfo&&) noexcept = default;
  AttachmentInfo(AttachmentInfo&) = default;

  // Makes 'image_usage_tracker' track the usage of this image.
  AttachmentInfo& AddToTracker(image::UsageTracker& image_usage_tracker,
                               const Image& sample_image) {
    image_usage_tracker.TrackImage(name_, sample_image);
    return *this;
  }

  // Performs following steps:
  // (1) Retrieve the initial usage from 'image_usage_tracker' and use it to
  //     construct a usage history.
  // (2) Call 'populate_history' to populate subpasses of the history.
  // (3) Add the history to 'graphics_pass' and records the attachment index.
  // (4) Update 'image_usage_tracker' to track the last usage of this image in
  //     'graphics_pass'.
  AttachmentInfo& AddToGraphicsPass(
      GraphicsPass& graphics_pass, image::UsageTracker& image_usage_tracker,
      const std::function<void(image::UsageHistory&)>& populate_history) {
    image::UsageHistory history{image_usage_tracker.GetUsage(name_)};
    populate_history(history);
    index_ = graphics_pass.AddImage(name_, std::move(history));
    graphics_pass.UpdateTrackedImageUsage(name_, image_usage_tracker);
    return *this;
  }

  // Informs 'graphics_pass' that this attachment will resolve to
  // 'target_attachment' at 'subpass'.
  AttachmentInfo& ResolveToAttachment(GraphicsPass& graphics_pass,
                                      const AttachmentInfo& target_attachment,
                                      int subpass) {
    graphics_pass.AddMultisampleResolving(name_, target_attachment.name_,
                                          subpass);
    return *this;
  }

  // Accessors.
  int index() const { return index_.value(); }

 private:
  // Image name. This is used to identify an image in GraphicsPass and
  // image::UsageTracker.
  std::string name_;

  // Attachment index. This is used to identify an image within a
  // VkAttachmentDescription array when constructing render passes.
  absl::optional<int> index_;
};

} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_ATTACHMENT_INFO_H */
