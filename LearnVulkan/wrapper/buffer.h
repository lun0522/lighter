//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_BUFFER_H
#define WRAPPER_VULKAN_BUFFER_H

#include <vector>

#include <vulkan/vulkan.hpp>

namespace wrapper {
namespace vulkan {

class Context;

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
  VertexBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const void* vertex_data, size_t vertex_size, size_t vertex_count,
            const void*  index_data, size_t  index_size, size_t  index_count);
  void Draw(const VkCommandBuffer& command_buffer) const;
  ~VertexBuffer();

  // This class is neither copyable nor movable
  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

 private:
  std::shared_ptr<Context> context_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
  VkDeviceSize vertex_size_;
  uint32_t vertex_count_, index_count_;
};

class UniformBuffer {
 public:
  UniformBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const void* data, size_t num_chunk, size_t chunk_size);
  void Update(size_t chunk_index) const;
  void Bind(const VkCommandBuffer& command_buffer,
            const VkPipelineLayout& pipeline_layout,
            size_t chunk_index) const;
  ~UniformBuffer();

  // This class is neither copyable nor movable
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  // TODO: temporary
  const std::vector<VkDescriptorSetLayout>& descriptor_set_layouts() const
      { return descriptor_set_layouts_; }

 private:
  std::shared_ptr<Context> context_;
  const char* data_;
  size_t chunk_memory_size_, chunk_data_size_;
  VkDescriptorPool descriptor_pool_;
  std::vector<VkDescriptorSetLayout> descriptor_set_layouts_;
  std::vector<VkDescriptorSet> descriptor_sets_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_BUFFER_H */
