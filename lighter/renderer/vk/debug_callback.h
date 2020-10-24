//
//  debug_callback.h
//
//  Created by Pujun Lun on 10/23/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_DEBUG_CALLBACK_H
#define LIGHTER_RENDERER_VK_DEBUG_CALLBACK_H

#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vk {

// Forward declarations.
class Context;

// Wraps VkDebugUtilsMessengerEXT, which relays debug messages from graphics
// drivers back to application.
class DebugCallback {
 public:
  DebugCallback(const Context* context, uint32_t message_severity,
                uint32_t message_type);

  // This class is neither copyable nor movable.
  DebugCallback(const DebugCallback&) = delete;
  DebugCallback& operator=(const DebugCallback&) = delete;

  ~DebugCallback();

  // Returns the names of required layers for validation support.
  static const std::vector<const char*>& GetRequiredLayers();

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque callback object.
  VkDebugUtilsMessengerEXT callback_;
};

} /* namespace vk */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VK_DEBUG_CALLBACK_H */
