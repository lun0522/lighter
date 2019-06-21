//
//  util.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_UTIL_H

#include "absl/strings/str_format.h"
#include "jessie_steamer/common/util.h"
#include "third_party/vulkan/vulkan.h"

#define ASSERT_SUCCESS(event, error)                      \
  if (event != VK_SUCCESS) {                              \
    FATAL(absl::StrFormat("Errno %d: %s", event, error))  \
  }

#define CONTAINER_SIZE(container) static_cast<uint32_t>(container.size())

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

constexpr uint32_t nullflag = 0;
constexpr uint32_t kPerVertexBindingPoint = 0;
constexpr uint32_t kPerInstanceBindingPointBase = 1;
constexpr int kCubemapImageCount = 6;

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_UTIL_H */
