//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef WRAPPER_VULKAN_BUFFER_H
#define WRAPPER_VULKAN_BUFFER_H

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "descriptor.h"

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

namespace buffer {

struct DataInfo {
  const void* data;
  size_t data_size;
  uint32_t unit_count;
};

struct ChunkInfo {
  const void* data;
  size_t chunk_size;
  size_t num_chunk;
};

struct ImageInfo{
  const void* data;
  VkFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t channel;

  VkExtent3D   extent()    const { return {width, height, /*depth=*/1}; }
  VkDeviceSize data_size() const { return width * height * channel; }
};

} /* namespace buffer */

class VertexBuffer {
 public:
  VertexBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const buffer::DataInfo& vertex_info,
            const buffer::DataInfo& index_info);
  void Draw(const VkCommandBuffer& command_buffer) const;
  ~VertexBuffer();

  // This class is neither copyable nor movable
  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

 private:
  std::shared_ptr<Context> context_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
  VkDeviceSize index_offset_;
  uint32_t index_count_;
};

class UniformBuffer {
 public:
  UniformBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const buffer::ChunkInfo& chunk_info,
            uint32_t binding_point,
            VkShaderStageFlags shader_stage);
  void Update(size_t chunk_index) const;
  void Bind(const VkCommandBuffer& command_buffer,
            const VkPipelineLayout& pipeline_layout,
            size_t chunk_index) const;
  ~UniformBuffer();

  // This class is neither copyable nor movable
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  const Descriptor& descriptor() const { return descriptor_; }

 private:
  std::shared_ptr<Context> context_;
  const char* data_;
  size_t chunk_memory_size_, chunk_data_size_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
  Descriptor descriptor_;
};

class ImageBuffer {
 public:
  ImageBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const buffer::ImageInfo& image_info);
  ~ImageBuffer();

  // This class is neither copyable nor movable
  ImageBuffer(const ImageBuffer&) = delete;
  ImageBuffer& operator=(const ImageBuffer&) = delete;

  const VkImage& image() const { return image_; }

 private:
  std::shared_ptr<Context> context_;
  VkImage image_;
  VkDeviceMemory device_memory_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_BUFFER_H */
