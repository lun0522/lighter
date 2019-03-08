//
//  command.h
//  LearnVulkan
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LEARNVULKAN_COMMAND_H
#define LEARNVULKAN_COMMAND_H

#include <functional>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "wrapper/basic_object.h" // TODO: remove wrapper/
#include "wrapper/buffer.h" // TODO: remove wrapper/

namespace vulkan {

class Application;
using namespace wrapper; // TODO: remove

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
  using RecordCommand = std::function<void (const VkCommandBuffer&)>;
  static void OneTimeCommand(
      const VkDevice& device,
      const Queues::Queue& queue,
      const RecordCommand& on_record);
  
  Command(const Application& app) : app_{app} {}
  VkResult DrawFrame();
  void Init();
  void Cleanup();
  ~Command();

  // This class is not copyable or movable
  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

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

} /* namespace vulkan */

#endif /* LEARNVULKAN_COMMAND_H */
