//
//  synchronization.h
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZATION_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZATION_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
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
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZATION_H */
