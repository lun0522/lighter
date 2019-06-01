//
//  context.cc
//
//  Created by Pujun Lun on 3/6/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

void Context::Init(const std::string& name, int width, int height) {
  if (is_first_time_) {
    window_.Init(name, {width, height});
    instance_.Init(ptr());
#ifndef NDEBUG
    // relay debug messages back to application
    callback_.Init(ptr(),
                   message_severity::kWarning
                       | message_severity::kError,
                   message_type::kGeneral
                       | message_type::kValidation
                       | message_type::kPerformance);
#endif /* !NDEBUG */
    surface_.Init(ptr());
    physical_device_.Init(ptr());
    device_.Init(ptr());
    is_first_time_ = false;
  }
  swapchain_.Init(ptr());
  render_pass_.Init(ptr());
}

void Context::Recreate() {
  // do nothing if window is minimized
  while (window_.IsMinimized()) {
    glfwWaitEvents();
  }
  vkDeviceWaitIdle(*device_);
  Cleanup();
  Init();
}

void Context::Cleanup() {
  render_pass_.Cleanup();
  swapchain_.Cleanup();
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
