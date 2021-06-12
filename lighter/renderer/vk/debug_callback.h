//
//  debug_callback.h
//
//  Created by Pujun Lun on 10/23/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_DEBUG_CALLBACK_H
#define LIGHTER_RENDERER_VK_DEBUG_CALLBACK_H

#include "lighter/renderer/type.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk {

// Forward declarations.
class Context;

// Wraps VkDebugUtilsMessengerEXT, which relays debug messages from graphics
// drivers back to application.
class DebugCallback {
 public:
  DebugCallback(const Context* context, const debug_message::Config& config);

  // This class is neither copyable nor movable.
  DebugCallback(const DebugCallback&) = delete;
  DebugCallback& operator=(const DebugCallback&) = delete;

  ~DebugCallback();

 private:
  // Context that holds basic wrapper objects.
  const Context& context_;

  // Opaque callback object.
  VkDebugUtilsMessengerEXT callback_ = VK_NULL_HANDLE;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_DEBUG_CALLBACK_H
