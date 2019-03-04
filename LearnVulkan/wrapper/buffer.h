//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef VULKAN_WRAPPER_BUFFER_H
#define VULKAN_WRAPPER_BUFFER_H

#include <vulkan/vulkan.hpp>

#include "util.h"

namespace vulkan {
class Application;  // TODO: move to namespace wrapper
} /* namespace vulkan */

namespace vulkan {
namespace wrapper {

/** VkBuffer represents linear arrays of data and configures usage of the data.
 *      Data can be transfered between buffers with the help of transfer queues.
 *
 *  Initialization:
 *      Data size
 *      Buffer usage (vertex/index buffer, src/dst of transfer)
 *      Shared by which device queues
 *      VkDevice
 *
 *------------------------------------------------------------------------------
 *
 *  VkDeviceMemory is a handle to the actual data stored in device memory.
 *      When we transfer data from host to device, we interact with
 *      VkPhysicalDevice rather than VkBuffer.
 *
 *  Initialization:
 *      Data size (can be different from the size we allocate for the buffer
 *          because of alignments)
 *      Memory type (visibility to host and device)
 *      VkBuffer that will be binded with it
 *      VkDevice
 *      VkPhysicalDevice
 */
class VertexBuffer {
 public:
  VertexBuffer(const Application& app) : app_{app} {}
  void Init(const void* vertex_data, size_t vertex_size, size_t vertex_count,
            const void*  index_data, size_t  index_size, size_t  index_count);
  void Draw(const VkCommandBuffer& command_buffer) const;
  ~VertexBuffer();
  MARK_NOT_COPYABLE_OR_MOVABLE(VertexBuffer);

 private:
  const Application& app_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
  VkDeviceSize vertex_size_;
  uint32_t vertex_count_, index_count_;
};

class UniformBuffer {
 public:
  UniformBuffer(const Application& app) : app_{app} {}
  void Init(const void* data, size_t num_chunk, size_t chunk_size);
  void Update(size_t chunk_index) const;
  ~UniformBuffer();
  MARK_NOT_COPYABLE_OR_MOVABLE(UniformBuffer);

  const VkDescriptorSetLayout& desc_set_layout() const
      { return desc_set_layout_; }  // TODO: temporary

 private:
  const Application& app_;
  const char* data_;
  size_t chunk_size_;
  VkDescriptorSetLayout desc_set_layout_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
};

} /* namespace wrapper */
} /* namespace vulkan */

#endif /* VULKAN_WRAPPER_BUFFER_H */
