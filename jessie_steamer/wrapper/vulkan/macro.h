//
//  macro.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_MACRO_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_MACRO_H

#include <stdexcept>

#include "absl/strings/str_format.h"
#include "third_party/vulkan/vulkan.h"

#define ASSERT_SUCCESS(event, error)                                          \
  if (event != VK_SUCCESS) {                                                  \
    throw std::runtime_error{absl::StrFormat("Error %d: %s", event, error)};  \
  }

#define CONTAINER_SIZE(container)                                             \
    static_cast<uint32_t>(container.size())

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

constexpr uint32_t nullflag = 0;

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_MACRO_H */
