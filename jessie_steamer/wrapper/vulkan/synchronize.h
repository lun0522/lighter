//
//  synchronize.h
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// Synchronization within the graphics device, possibly across queues.
class Semaphores {
 public:
  Semaphores() = default;

  // This class is neither copyable nor movable.
  Semaphores(const Semaphores&) = delete;
  Semaphores& operator=(const Semaphores&) = delete;

  ~Semaphores();

  // Initializes semaphores of 'count'.
  void Init(SharedBasicContext context, int count);

  // Accessors.
  const VkSemaphore& operator[](int index) const { return semas_.at(index); }

 private:
  // Pointer to context.
  SharedBasicContext context_;

  // Opaque semaphore objects.
  std::vector<VkSemaphore> semas_;
};

// Synchronization between the host and device. Designed for the host waiting
// for device.
class Fences {
 public:
  Fences() = default;

  // This class is neither copyable nor movable.
  Fences(const Fences&) = delete;
  Fences& operator=(const Fences&) = delete;

  ~Fences();

  // Initializes fences of 'count' with the initial state 'is_signaled'.
  void Init(SharedBasicContext context, int count, bool is_signaled);

  // Accessors.
  const VkFence& operator[](int index) const { return fences_.at(index); }

 private:
  // Pointer to context.
  SharedBasicContext context_;

  // Opaque fence objects.
  std::vector<VkFence> fences_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H */
