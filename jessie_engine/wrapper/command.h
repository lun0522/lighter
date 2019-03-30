//
//  command.h
//
//  Created by Pujun Lun on 2/9/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_COMMAND_H
#define WRAPPER_VULKAN_COMMAND_H

#include <functional>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "basic_object.h"
#include "buffer.h"
#include "pipeline.h"
#include "synchronize.h"

namespace wrapper {
namespace vulkan {

class Context;

namespace command {

using OneTimeRecordCommand = std::function<void(
    const VkCommandBuffer& command_buffer)>;
using MultiTimeRecordCommand = std::function<void(
    const VkCommandBuffer& command_buffer, size_t image_index)>;
using UpdateDataFunc = std::function<void (size_t image_index)>;

void OneTimeCommand(std::shared_ptr<Context> context,
                    const Queues::Queue& queue,
                    const OneTimeRecordCommand& on_record);

} /* namespace command */

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
  Command() = default;
  VkResult DrawFrame(size_t current_frame,
                     const command::UpdateDataFunc& update_func);
  void Init(std::shared_ptr<Context> context,
            size_t num_frame,
            const command::MultiTimeRecordCommand& on_record);
  void Cleanup();
  ~Command();

  // This class is neither copyable nor movable
  Command(const Command&) = delete;
  Command& operator=(const Command&) = delete;

 private:
  std::shared_ptr<Context> context_;
  bool is_first_time_{true};
  Semaphores image_available_semas_;
  Semaphores render_finished_semas_;
  Fences in_flight_fences_;
  VkCommandPool command_pool_;
  std::vector<VkCommandBuffer> command_buffers_;

  void RecordCommand(const command::MultiTimeRecordCommand& on_record);
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_COMMAND_H */
