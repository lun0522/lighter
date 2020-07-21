//
//  image_util.h
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H

#include "lighter/common/util.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

// Describes how we are using an image.
class Usage {
 public:
  // Usage types of images that we can handle.
  enum class UsageType {
    // Don't care about the content stored in the image.
    kDontCare,
    // Color attachment.
    kRenderTarget,
    // Depth stencil attachment.
    kDepthStencil,
    // Present to screen.
    kPresentation,
    // Linearly accessed.
    kLinearAccess,
    // Sampled as texture.
    kSample,
    // Used for transferring image data within the device, e.g. blitting one
    // image to another.
    kTransfer,
  };

  // Whether to read and/or write.
  enum class AccessType {
    kDontCare,
    kReadOnly,
    kWriteOnly,
    kReadWrite,
  };

  // Where to access the image. Note that kOther is different from kDontCare.
  // For example, depth stencil attachments are actually not written in fragment
  // shader. It has its own pipeline stages.
  enum class AccessLocation {
    kDontCare,
    kHost,
    kFragmentShader,
    kComputeShader,
    kOther,
  };

  // Convenience function to return Usage for images sampled as textures in
  // fragment shaders.
  static Usage GetSampledInFragmentShaderUsage() {
    return Usage{UsageType::kSample, AccessType::kReadOnly,
                 AccessLocation::kFragmentShader};
  }

  // Convenience function to return Usage for images used as render targets.
  static Usage GetRenderTargetUsage() {
    return Usage{UsageType::kRenderTarget, AccessType::kWriteOnly,
                 AccessLocation::kOther};
  }

  // Convenience function to return Usage for images used as depth stencil
  // attachment.
  static Usage GetDepthStencilUsage() {
    return Usage{UsageType::kDepthStencil, AccessType::kReadWrite,
                 AccessLocation::kOther};
  }

  // Convenience function to return Usage for images to be presented to screen.
  static Usage GetPresentationUsage() {
    return Usage{UsageType::kPresentation, AccessType::kReadOnly,
                 AccessLocation::kOther};
  }

  // Convenience function to return Usage for images linearly accessed in
  // compute shaders.
  static Usage GetLinearAccessInComputeShaderUsage(AccessType access_type) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type");
    return Usage{UsageType::kLinearAccess, access_type,
                 AccessLocation::kComputeShader};
  }

  explicit Usage() : Usage{UsageType::kDontCare, AccessType::kDontCare,
                           AccessLocation::kDontCare} {}

  // Returns VkAccessFlags used for inserting image memory barriers.
  VkAccessFlags GetAccessFlags() const;

  // Returns VkPipelineStageFlags used for inserting image memory barriers.
  VkPipelineStageFlags GetPipelineStageFlags() const;

  // Returns which VkImageLayout should be used for this usage.
  VkImageLayout GetImageLayout() const;

  // Returns VkImageUsageFlagBits for this usage. Note that this must not be
  // called if 'usage_type' is UsageType::kDontCare, since it doesn't have
  // corresponding flag bits.
  VkImageUsageFlagBits GetImageUsageFlagBits() const;

  // In most cases we only need 8-bit integers for each image channel.
  // If this is called, we would use 16-bit floats instead.
  Usage& set_use_high_precision() {
    use_high_precision_ = true;
    return *this;
  }

  // Overrides.
  bool operator==(const Usage& other) const {
    return usage_type_ == other.usage_type_ &&
           access_type_ == other.access_type_ &&
           access_location_ == other.access_location_;
  }

  // Accessors.
  UsageType usage_type() const { return usage_type_; }
  AccessType access_type() const { return access_type_; }
  AccessLocation access_location() const { return access_location_; }
  bool use_high_precision() const { return use_high_precision_; }

 private:
  // We make this constructor private so that the user can only construct the
  // default usage or use static methods to construct usages that are guaranteed
  // to be valid.
  Usage(UsageType usage_type,
        AccessType access_type,
        AccessLocation access_location)
      : usage_type_{usage_type}, access_type_{access_type},
        access_location_{access_location}, use_high_precision_{false} {}

  UsageType usage_type_;
  AccessType access_type_;
  AccessLocation access_location_;
  bool use_high_precision_;
};

// Returns true if any of 'usages' is UsageType::kLinearAccess.
bool IsLinearAccessed(absl::Span<const Usage> usages);

// Returns true if any of 'usages' is in high precision.
bool UseHighPrecision(absl::Span<const Usage> usages);

// Returns VkImageUsageFlags that contains all usages.
VkImageUsageFlags GetImageUsageFlags(absl::Span<const Usage> usages);

} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H */
