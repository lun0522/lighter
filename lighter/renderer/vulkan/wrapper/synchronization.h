//
//  synchronization.h
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_WRAPPER_SYNCHRONIZATION_H
#define LIGHTER_RENDERER_VULKAN_WRAPPER_SYNCHRONIZATION_H

#include <vector>

#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Synchronization within the graphics device, possibly across queues.
class Semaphores {
 public:
  Semaphores(SharedBasicContext context, int count);

  // This class is neither copyable nor movable.
  Semaphores(const Semaphores&) = delete;
  Semaphores& operator=(const Semaphores&) = delete;

  ~Semaphores();

  // Accessors.
  const VkSemaphore& operator[](int index) const { return semas_.at(index); }

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque semaphore objects.
  std::vector<VkSemaphore> semas_;
};

// Synchronization between the host and device. Designed for the host waiting
// for device.
class Fences {
 public:
  Fences(SharedBasicContext context, int count, bool is_signaled);

  // This class is neither copyable nor movable.
  Fences(const Fences&) = delete;
  Fences& operator=(const Fences&) = delete;

  ~Fences();

  // Accessors.
  const VkFence& operator[](int index) const { return fences_.at(index); }

 private:
  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque fence objects.
  std::vector<VkFence> fences_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_WRAPPER_SYNCHRONIZATION_H */
