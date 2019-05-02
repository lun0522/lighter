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
  void Init(const std::shared_ptr<Context>& context,
            size_t count);
  ~Semaphores();

  // This class is neither copyable nor movable
  Semaphores(const Semaphores&) = delete;
  Semaphores& operator=(const Semaphores&) = delete;

  const VkSemaphore& operator[](size_t index) const { return semas_[index]; }

 private:
  std::shared_ptr<Context> context_;
  std::vector<VkSemaphore> semas_;
};

class Fences {
 public:
  Fences() = default;
  void Init(const std::shared_ptr<Context>& context,
            size_t count,
            bool is_signaled);
  ~Fences();

  // This class is neither copyable nor movable
  Fences(const Fences&) = delete;
  Fences& operator=(const Fences&) = delete;

  const VkFence& operator[](size_t index) const { return fences_[index]; }

 private:
  std::shared_ptr<Context> context_;
  std::vector<VkFence> fences_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_SYNCHRONIZE_H */
