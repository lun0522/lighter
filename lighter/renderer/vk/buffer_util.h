//
//  buffer_util.h
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_BUFFER_UTIL_H
#define LIGHTER_RENDERER_VK_BUFFER_UTIL_H

#include "lighter/renderer/buffer_usage.h"
#include "third_party/absl/types/span.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace buffer {

// Returns VkBufferUsageFlags that contains all usages.
VkBufferUsageFlags GetBufferUsageFlags(absl::Span<const BufferUsage> usages);

} /* namespace buffer */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_BUFFER_UTIL_H */
