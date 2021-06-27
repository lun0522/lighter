//
//  image_util.h
//
//  Created by Pujun Lun on 4/9/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H

#include "lighter/renderer/ir/image_usage.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace image {

using ir::AccessLocation;
using ir::AccessType;
using ir::ImageUsage;

// Returns VkAccessFlags used for inserting image memory barriers.
VkAccessFlags GetAccessFlags(const ImageUsage& usage);

// Returns VkPipelineStageFlags used for inserting image memory barriers.
VkPipelineStageFlags GetPipelineStageFlags(const ImageUsage& usage);

// Returns which VkImageLayout should be used for 'usage'.
VkImageLayout GetImageLayout(const ImageUsage& usage);

// Returns VkImageUsageFlagBits for 'usage'. Note that this must not be called
// if usage type is kDontCare, since it doesn't have corresponding flag bits.
VkImageUsageFlagBits GetImageUsageFlagBits(const ImageUsage& usage);

// Returns VkImageUsageFlags that contains all usages.
VkImageUsageFlags GetImageUsageFlags(absl::Span<const ImageUsage> usages);

// Returns whether we need to explicitly synchronize image memory access when
// the image usage changes, which means to insert memory barriers in compute
// pass, or add subpass dependencies in graphics pass.
bool NeedSynchronization(const ImageUsage& prev_usage,
                         const ImageUsage& curr_usage);

} /* namespace image */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_IMAGE_UTIL_H */
