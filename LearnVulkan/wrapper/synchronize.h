//
//  synchronize.h
//
//  Created by Pujun Lun on 3/1/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_SYNCHRONIZE_H
#define VULKAN_WRAPPER_SYNCHRONIZE_H

#include <vector>

#include <vulkan/vulkan.hpp>

namespace vulkan {
namespace wrapper {

class Context;

/** VkSemaphore and VkFence are used for synchronization. Their constructions
 *      only requires VkDevice.
 */

class Semaphores {
 public:
  Semaphores() = default;
  void Init(std::shared_ptr<Context> context, size_t count);
  ~Semaphores();

  // This class is neither copyable nor movable
  Semaphores(const Semaphores&) = delete;
  Semaphores& operator=(const Semaphores&) = delete;

  VkSemaphore& operator[](size_t index) { return semas_[index]; }
  const VkSemaphore& operator[](size_t index) const { return semas_[index]; }

 private:
  std::shared_ptr<Context> context_;
  std::vector<VkSemaphore> semas_;
};

class Fences {
 public:
  Fences() = default;
  void Init(std::shared_ptr<Context> context, size_t count, bool is_signaled);
  ~Fences();

  // This class is neither copyable nor movable
  Fences(const Fences&) = delete;
  Fences& operator=(const Fences&) = delete;

  VkFence& operator[](size_t index) { return fences_[index]; }
  const VkFence& operator[](size_t index) const { return fences_[index]; }

 private:
  std::shared_ptr<Context> context_;
  std::vector<VkFence> fences_;
};

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_SYNCHRONIZE_H */
