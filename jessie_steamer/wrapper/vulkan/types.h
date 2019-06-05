//
//  basic_object.h
//
//  Created by Pujun Lun on 6/4/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TYPES_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TYPES_H

#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkQueue is the queue associated with the logical device. When we create it,
 *    we can specify both queue family index and queue index (within family).
 */
struct Queues {
  struct Queue {
    VkQueue queue;
    uint32_t family_index;
  };
  Queue graphics, transfer, present;

  Queues() = default;

  // This class is neither copyable nor movable
  Queues(const Queues&) = delete;
  Queues& operator=(const Queues&) = delete;

  ~Queues() = default;  // implicitly cleaned up with physical device

  void set_queues(const VkQueue& graphics_queue, const VkQueue& present_queue) {
    graphics.queue = graphics_queue;
    transfer.queue = graphics_queue;
    present.queue = present_queue;
  }

  void set_family_indices(uint32_t graphics_index, uint32_t present_index) {
    graphics.family_index = graphics_index;
    transfer.family_index = graphics_index;
    present.family_index = present_index;
  }
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_TYPES_H */
