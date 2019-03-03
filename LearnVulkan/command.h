//
//  command.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_COMMAND_H
#define LEARNVULKAN_COMMAND_H

#include <vector>

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {

class Application;

/** VkCommandPool allocates command buffer memory.
 *
 *  Initialization:
 *      Queue family index
 *
 *------------------------------------------------------------------------------
 *
 *  VkCommandBuffer records all operations we want to perform and submit to a
 *      device queue for execution. Primary level command buffers can call
 *      secondary level ones and submit to queues, while secondary levels ones
 *      are not directly submitted.
 *
 *  Initialization:
 *      VkCommandPool
 *      Level (either primary or secondary)
 */
class Command {
 public:
  Command(const Application& app) : app_{app} {}
  VkResult DrawFrame();
  void Init();
  void Cleanup();
  ~Command();
  MARK_NOT_COPYABLE_OR_MOVABLE(Command);

 private:
  const Application& app_;
  size_t current_frame_{0};
  bool is_first_time_{true};
  std::vector<VkSemaphore> image_available_semas_;
  std::vector<VkSemaphore> render_finished_semas_;
  std::vector<VkFence> in_flight_fences_;
  VkCommandPool command_pool_;
  std::vector<VkCommandBuffer> command_buffers_;
};

VkCommandPool CreateCommandPool(
    uint32_t queue_family_index,
    const VkDevice& device,
    bool is_transient = false);

VkCommandBuffer CreateCommandBuffer(
    const VkDevice& device,
    const VkCommandPool& pool);

std::vector<VkCommandBuffer> CreateCommandBuffers(
    size_t count,
    const VkDevice& device,
    const VkCommandPool& command_pool);

} /* namespace vulkan */

#endif /* LEARNVULKAN_COMMAND_H */
