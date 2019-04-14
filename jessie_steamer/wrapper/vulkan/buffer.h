//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H

#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

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
  struct Info {
    struct Subfield {
      const void* data;
      size_t data_size;
      uint32_t unit_count;
    };
    Subfield vertices, indices;
  };

  VertexBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const std::vector<Info>& infos);
  void Draw(const VkCommandBuffer& command_buffer) const;
  ~VertexBuffer();

  // This class is only movable
  VertexBuffer(VertexBuffer&&) = default;
  VertexBuffer& operator=(VertexBuffer&&) = default;

 private:
  struct Segment {
    VkDeviceSize vertices_offset;
    VkDeviceSize indices_offset;
    uint32_t indices_count;
  };

  std::shared_ptr<Context> context_;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
  std::vector<Segment> segments_;
};

class UniformBuffer {
 public:
  struct Info {
    const void* data;
    size_t chunk_size;
    size_t num_chunk;
  };

  UniformBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const Info& info);
  void Update(size_t chunk_index) const;
  ~UniformBuffer();

  // This class is only movable
  UniformBuffer(UniformBuffer&&) = default;
  UniformBuffer& operator=(UniformBuffer&&) = default;

  VkDescriptorBufferInfo descriptor_info(size_t chunk_index) const;

 private:
  std::shared_ptr<Context> context_;
  const char* data_ = nullptr;
  size_t chunk_memory_size_ = 0, chunk_data_size_ = 0;
  VkBuffer buffer_;
  VkDeviceMemory device_memory_;
};

class TextureBuffer {
 public:
  struct Info{
    bool is_cubemap;
    std::vector<const void*> datas;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;

    VkExtent3D extent() const { return {width, height, /*depth=*/1}; }
    VkDeviceSize data_size() const {
      return datas.size() * width * height * channel;
    }
  };

  TextureBuffer() = default;
  void Init(std::shared_ptr<Context> context,
            const Info& info);
  ~TextureBuffer();

  // This class is only movable
  TextureBuffer(TextureBuffer&&) = default;
  TextureBuffer& operator=(TextureBuffer&&) = default;

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

  // This class is only movable
  DepthStencilBuffer(DepthStencilBuffer&&) = default;
  DepthStencilBuffer& operator=(DepthStencilBuffer&&) = default;

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
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H */
