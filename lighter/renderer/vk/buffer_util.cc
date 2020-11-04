//
//  buffer_util.cc
//
//  Created by Pujun Lun on 11/3/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/buffer_util.h"

#include "lighter/renderer/type.h"
#include "lighter/renderer/vk/util.h"

namespace lighter {
namespace renderer {
namespace vk {
namespace buffer {
namespace {

using UsageType = BufferUsage::UsageType;

// Returns VkBufferUsageFlags for 'usage'. Note that this must not be called
// if usage type is kDontCare, since it doesn't have corresponding flag bits.
VkBufferUsageFlags GetBufferUsageFlags(const BufferUsage& usage) {
  switch (usage.usage_type()) {
    case UsageType::kDontCare:
      FATAL("No corresponding buffer usage flags for usage type kDontCare");

    case UsageType::kVertexWithoutIndex:
      return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    case UsageType::kVertexWithIndex:
      return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                 | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    case UsageType::kUniform:
      return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    case UsageType::kTransfer:
      switch (usage.access_type()) {
        case AccessType::kDontCare:
        case AccessType::kReadWrite:
          FATAL("Access type must not be kDontCare or kReadWrite for usage "
                "type kTransfer");

        case AccessType::kReadOnly:
          return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        case AccessType::kWriteOnly:
          return VK_BUFFER_USAGE_TRANSFER_DST_BIT;
      }
  }
}

} /* namespace */

VkBufferUsageFlags GetBufferUsageFlags(absl::Span<const BufferUsage> usages) {
  auto flags = nullflag;
  for (const auto& usage : usages) {
    if (usage.usage_type() != UsageType::kDontCare) {
      flags |= GetBufferUsageFlags(usage);
    }
  }
  return static_cast<VkImageUsageFlags>(flags);
}

} /* namespace buffer */
} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
