//
//  image_util.h
//
//  Created by Pujun Lun on 4/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H

#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

// Describes how we are using an image.
struct Usage {
  // Usage types of images that we can handle.
  enum class UsageType {
    // Don't care about the content stored in the image.
    kDontCare,
    // Color attachment that we can render to.
    kRenderTarget,
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

  // For UsageType::kLinearAccess, this must not be kDontCare.
  // For UsageType::kTransfer, this must be either kReadOnly or kWriteOnly.
  // For other usage types, this will be ignored.
  enum class AccessType {
    kDontCare,
    kReadOnly,
    kWriteOnly,
    kReadWrite,
  };

  // For UsageType::kLinearAccess, this must not be kDontCare.
  // For UsageType::kSample, this must be either kFragmentShader or
  // kComputeShader.
  // For other usage types, this will be ignored.
  enum class AccessLocation {
    kDontCare,
    kHost,
    kFragmentShader,
    kComputeShader,
  };

  // Convenience function to return Usage for images sampled as textures in
  // fragment shaders.
  static Usage GetSampledInFragmentShaderUsage() {
    return Usage{UsageType::kSample,
                 /*access_type=*/AccessType::kDontCare,
                 AccessLocation::kFragmentShader};
  }

  // Convenience function to return Usage for images used as render targets.
  static Usage GetRenderTargetUsage() {
    return Usage{UsageType::kRenderTarget};
  }

  // Convenience function to return Usage for images linearly accessed in
  // compute shaders.
  static Usage GetLinearAccessInComputeShaderUsage(AccessType access_type) {
    return Usage{UsageType::kLinearAccess, access_type,
                 AccessLocation::kComputeShader};
  }

  // In most cases we only need 8-bit integers for each image channel.
  // If 'use_high_precision' is true, we would use 16-bit floats instead.
  explicit Usage(UsageType usage_type = UsageType::kDontCare,
                 AccessType access_type = AccessType::kDontCare,
                 AccessLocation access_location = AccessLocation::kDontCare,
                 bool use_high_precision = false)
      : usage_type{usage_type}, access_type{access_type},
        access_location{access_location},
        use_high_precision{use_high_precision} {}

  // Throws a runtime exception if this usage is invalid.
  void Validate() const;

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

  // Overrides.
  bool operator==(const Usage& other) const {
    return usage_type == other.usage_type &&
           access_type == other.access_type &&
           access_location == other.access_location;
  }

  // Modifiers.
  Usage& set_use_high_precision() {
    use_high_precision = true;
    return *this;
  }

  UsageType usage_type;
  AccessType access_type;
  AccessLocation access_location;
  bool use_high_precision;
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
