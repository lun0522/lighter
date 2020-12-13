//
//  image_usage.h
//
//  Created by Pujun Lun on 10/11/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IMAGE_USAGE_H
#define LIGHTER_RENDERER_IMAGE_USAGE_H

#include <algorithm>

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {

// Describes how we would use an image.
class ImageUsage {
 public:
  // TODO: Consider VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT.
  // Usage types of images that we can handle.
  enum class UsageType {
    // Don't care about the content stored in the image.
    kDontCare,
    // Color attachment.
    kRenderTarget,
    // Depth stencil attachment.
    kDepthStencil,
    // A multisample image resolves to a single sample image.
    kMultisampleResolve,
    // Presented to screen.
    kPresentation,
    // Linearly accessed.
    kLinearAccess,
    // Only the value stored at the same pixel is accessed.
    kInputAttachment,
    // Sampled as texture.
    kSample,
    // Used for transferring image data within the device, e.g. blitting one
    // image to another.
    kTransfer,
  };

  // Convenience function to return usage for images sampled as textures in
  // fragment shaders.
  static ImageUsage GetSampledInFragmentShaderUsage() {
    return ImageUsage{UsageType::kSample, AccessType::kReadOnly,
                      AccessLocation::kFragmentShader};
  }

  // Convenience function to return usage for images used as render targets.
  static ImageUsage GetRenderTargetUsage(int attachment_location) {
    return ImageUsage{UsageType::kRenderTarget, AccessType::kReadWrite,
                      AccessLocation::kOther, attachment_location};
  }

  // Convenience function to return usage for images that we resolve multisample
  // images to.
  static ImageUsage GetMultisampleResolveTargetUsage() {
    return ImageUsage{UsageType::kMultisampleResolve, AccessType::kWriteOnly,
                      AccessLocation::kOther};
  }

  // Convenience function to return usage for images used as depth stencil
  // attachment.
  static ImageUsage GetDepthStencilUsage(AccessType access_type) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type");
    return ImageUsage{UsageType::kDepthStencil, access_type,
                      AccessLocation::kOther};
  }

  // Convenience function to return usage for images to be presented to screen.
  static ImageUsage GetPresentationUsage() {
    return ImageUsage{UsageType::kPresentation, AccessType::kReadOnly,
                      AccessLocation::kOther};
  }

  // Convenience function to return usage for images used as input attachments.
  static ImageUsage GetInputAttachmentUsage() {
    return ImageUsage{UsageType::kInputAttachment, AccessType::kReadOnly,
                      AccessLocation::kFragmentShader};
  }

  // Convenience function to return usage for images linearly accessed in
  // compute shaders.
  static ImageUsage GetLinearAccessInComputeShaderUsage(
      AccessType access_type) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type");
    return ImageUsage{UsageType::kLinearAccess, access_type,
                      AccessLocation::kComputeShader};
  }

  explicit ImageUsage() : ImageUsage{UsageType::kDontCare,
                                     AccessType::kDontCare,
                                     AccessLocation::kDontCare} {}

  // Returns true if any of 'usages' is UsageType::kLinearAccess.
  static bool IsLinearAccessed(absl::Span<const ImageUsage> usages) {
    return std::any_of(usages.begin(), usages.end(),
                       [](const ImageUsage& usage) {
                         return usage.usage_type() == UsageType::kLinearAccess;
                       });
  }

  // Overrides.
  bool operator==(const ImageUsage& other) const {
    return usage_type_ == other.usage_type_ &&
           access_type_ == other.access_type_ &&
           access_location_ == other.access_location_;
  }

  // Accessors.
  UsageType usage_type() const { return usage_type_; }
  AccessType access_type() const { return access_type_; }
  AccessLocation access_location() const { return access_location_; }
  int attachment_location() const { return attachment_location_.value(); }

 private:
  // We make this constructor private so that the user can only construct the
  // default usage or use static methods to construct usages that are guaranteed
  // to be valid.
  ImageUsage(UsageType usage_type, AccessType access_type,
             AccessLocation access_location,
             absl::optional<int> attachment_location = absl::nullopt)
      : usage_type_{usage_type}, access_type_{access_type},
        access_location_{access_location},
        attachment_location_{attachment_location} {}

  UsageType usage_type_;
  AccessType access_type_;
  AccessLocation access_location_;
  absl::optional<int> attachment_location_;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_IMAGE_USAGE_H */
