//
//  command.h
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H

#include <functional>
#include <memory>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_object.h"
#include "jessie_steamer/wrapper/vulkan/synchronize.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Context;

/** VkCommandPool allocates command buffer memory.
 *
 *  Initialization:
 *    Queue family index
 *
 *------------------------------------------------------------------------------
 *
 *  VkCommandBuffer records all operations we want to perform and submit to a
 *    device queue for execution. Primary level command buffers can call
 *    secondary level ones and submit to queues, while secondary levels ones
 *    are not directly submitted.
 *
 *  Initialization:
 *    VkCommandPool
 *    Level (either primary or secondary)
 */
class Command {
 public:
  using OneTimeRecord = std::function<void(
      const VkCommandBuffer& command_buffer)>;
  using MultiTimeRecord = std::function<void(
      const VkCommandBuffer& command_buffer, const VkFramebuffer& framebuffer)>;
  using UpdateDataFunc = std::function<void (int current_frame)>;

  Command() = default;

  // This class is neither copyable nor movable
  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

  ~Command();

  static void OneTimeCommand(const std::shared_ptr<Context>& context,
                             const Queues::Queue& queue,
                             const OneTimeRecord& on_record);

  void Init(std::shared_ptr<Context> context, int num_frame);
  VkResult Draw(int current_frame,
                const UpdateDataFunc& update_data,
                const MultiTimeRecord& on_record);
  void Cleanup();

 private:
  std::shared_ptr<Context> context_;
  bool is_first_time_ = true;
  Semaphores image_available_semas_;
  Semaphores render_finished_semas_;
  Fences in_flight_fences_;
  VkCommandPool command_pool_;
  std::vector<VkCommandBuffer> command_buffers_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_COMMAND_H */
