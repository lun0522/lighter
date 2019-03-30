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
  bool is_cubemap;
  std::vector<const void*> datas;
  VkFormat format;
  uint32_t width;
  uint32_t height;
  uint32_t channel;

  VkExtent3D extent() const { return {width, height, /*depth=*/1}; }
  VkDeviceSize data_size() const {
    int batch = is_cubemap ? 6 : 1;
    return batch* width * height * channel;
  }
};

} /* namespace buffer */

class Context;

/** VkBuffer represents linear arrays of data and configures usage of the data.
 *    Data can be transfered between buffers with the help of transfer queues.
 *    For buffers that contain large amount of data and do not change very
 *    often, we will create a staging buffer (which is visible to both host and
 *    device, and thus is not the most effient for device) and a final buffer
 *    (which is only visible to device, and thus is optimal for device access).
 *    The staging buffer will only be used to transfer data to the final buffer,
 *    and then it will be destroyed.
 *
 *  Initialization:
 *    VkDevice
 *    Data size
 *    Buffer usage (vertex/index buffer, src/dst of transfer)
 *    Shared by which device queues
 *
 *------------------------------------------------------------------------------
 *
 *  VkDeviceMemory is a handle to the actual data stored in device memory.
 *    When we transfer data from host to device, we interact with
 *    VkPhysicalDevice rather than VkBuffer.
 *
 *  Initialization:
 *    Data size (can be different from the size we allocate for the buffer
 *      because of alignments)
 *    Memory type (visibility to host and device)
 *    VkBuffer that will be binded with it
 *    VkDevice
 *    VkPhysicalDevice
 */
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
            const buffer::ChunkInfo& chunk_info);
  void Update(size_t chunk_index) const;
  ~UniformBuffer();

  // This class is neither copyable nor movable
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  VkDescriptorBufferInfo descriptor_info(size_t chunk_index) const;

 private:
  std::shared_ptr<Context> context_;
  const char* data_;
  size_t chunk_memory_size_, chunk_data_size_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
};

class TextureBuffer {
 public:
  TextureBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const buffer::ImageInfo& image_info);
  ~TextureBuffer();

  // This class is neither copyable nor movable
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;

  const VkImage& image() const { return image_; }

 private:
  std::shared_ptr<Context> context_;
  VkImage image_;
  VkDeviceMemory device_memory_;
};

class DepthStencilBuffer {
 public:
  DepthStencilBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            VkExtent2D extent);
  void Cleanup();
  ~DepthStencilBuffer() { Cleanup(); }

  // This class is neither copyable nor movable
  DepthStencilBuffer(const DepthStencilBuffer&) = delete;
  DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;

  const VkImage& image() const { return image_; }
  VkFormat format()      const { return format_; }

 private:
  std::shared_ptr<Context> context_;
  VkImage image_;
  VkDeviceMemory device_memory_;
  VkFormat format_;
};

} /* namespace vulkan */
} /* namespace wrapper */

#endif /* WRAPPER_VULKAN_BUFFER_H */
