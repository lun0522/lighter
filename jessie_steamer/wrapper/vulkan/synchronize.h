//
//  synchronize.h
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H

#include <memory>
#include <vector>

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

/** VkSemaphore and VkFence are used for synchronization. Their constructions
 *    only requires VkDevice. Both of them can only be signaled by GPU, but
 *    fences can only be waited on by CPU (GPU->CPU sync) while semaphores
 *    can only be waited on by GPU (GPU->GPU sync, possibly across queues).
 */

class Semaphores {
 public:
  Semaphores() = default;

  // This class is neither copyable nor movable
  Semaphores(const Semaphores&) = delete;
  Semaphores& operator=(const Semaphores&) = delete;

  ~Semaphores();

  void Init(std::shared_ptr<Context> context, int count);

  const VkSemaphore& operator[](int index) const { return semas_[index]; }

 private:
  std::shared_ptr<Context> context_;
  std::vector<VkSemaphore> semas_;
};

class Fences {
 public:
  Fences() = default;

  // This class is neither copyable nor movable
  Fences(const Fences&) = delete;
  Fences& operator=(const Fences&) = delete;

  ~Fences();

  void Init(std::shared_ptr<Context> context, int count, bool is_signaled);

  const VkFence& operator[](int index) const { return fences_[index]; }

 private:
  std::shared_ptr<Context> context_;
  std::vector<VkFence> fences_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H */
