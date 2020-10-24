//
//  swapchain.cc
//
//  Created by Pujun Lun on 10/24/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vk/swapchain.h"

namespace lighter {
namespace renderer {
namespace vk {

const std::vector<const char*>& Swapchain::GetRequiredExtensions() {
  static const std::vector<const char*>* required_extensions = nullptr;
  if (required_extensions == nullptr) {
    required_extensions = new std::vector<const char*>{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
  }
  return *required_extensions;
}

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */
