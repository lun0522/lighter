//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H

#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

/** VkBuffer represents linear arrays of data and configures usage of the data.
 *    Data can be transferred between buffers with the help of transfer queues.
 *    For buffers that contain large amount of data and do not change very
 *    often, we will create a staging buffer (which is visible to both host and
 *    device, and thus is not the most efficient for device) and a final buffer
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
 *    VkBuffer that will be bound with it
 *    VkDevice
 *    VkPhysicalDevice
 */
class Buffer {
 public:
  struct CopyInfo {
    const void* data;
    VkDeviceSize size;
    VkDeviceSize offset;
  };

  explicit Buffer(SharedBasicContext context) : context_{std::move(context)} {}
  virtual ~Buffer() {
    vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
  }

 protected:
  SharedBasicContext context_;
  VkDeviceMemory device_memory_;
};

class DataBuffer : public Buffer {
 public:
  // Inherits constructor.
  using Buffer::Buffer;

  ~DataBuffer() override {
    vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  }

 protected:
  void CopyHostData(const std::vector<CopyInfo>& copy_infos,
                    size_t total_size);

  VkBuffer buffer_;
};

class PerVertexBuffer : public DataBuffer {
 public:
  struct Info {
    struct Field {
      const void* data;
      size_t data_size;
      uint32_t unit_count;
    };
    Field vertices, indices;
  };

  // Inherits constructor.
  using DataBuffer::DataBuffer;

  // This class is neither copyable nor movable.
  PerVertexBuffer(const PerVertexBuffer&) = delete;
  PerVertexBuffer& operator=(const PerVertexBuffer&) = delete;

  void Init(const std::vector<Info>& infos);
  void Draw(const VkCommandBuffer& command_buffer,
            int mesh_index, uint32_t instance_count) const;

 private:
  struct MeshData {
    VkDeviceSize vertices_offset;
    VkDeviceSize indices_offset;
    uint32_t indices_count;
  };
  std::vector<MeshData> mesh_datas_;
};

class PerInstanceBuffer : public DataBuffer {
 public:
  // Inherits constructor.
  using DataBuffer::DataBuffer;

  // This class is neither copyable nor movable.
  PerInstanceBuffer(const PerInstanceBuffer&) = delete;
  PerInstanceBuffer& operator=(const PerInstanceBuffer&) = delete;

  void Init(const void* data, size_t data_size);
  void Bind(const VkCommandBuffer& command_buffer,
            uint32_t binding_point) const;
};

class UniformBuffer : public DataBuffer {
 public:
  // Inherits constructor.
  using DataBuffer::DataBuffer;

  // This class is neither copyable nor movable.
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  ~UniformBuffer() override { delete data_; }

  void Init(size_t chunk_size, int num_chunk);
  void Flush(int chunk_index) const;

  template <typename DataType>
  DataType* data(int chunk_index) const {
    return reinterpret_cast<DataType*>(data_ + chunk_data_size_ * chunk_index);
  }

  VkDescriptorBufferInfo descriptor_info(int chunk_index) const;

 private:
  char* data_;
  size_t chunk_memory_size_, chunk_data_size_;
};

class ImageBuffer : public Buffer {
 public:
  // Inherits constructor.
  using Buffer::Buffer;

  ~ImageBuffer() override {
    vkDestroyImage(*context_->device(), image_, context_->allocator());
  }

  const VkImage& image() const { return image_; }

 protected:
  VkImage image_;
};

class TextureBuffer : public ImageBuffer {
 public:
  struct Info{
    VkExtent3D extent() const { return {width, height, /*depth=*/1}; }
    VkDeviceSize data_size() const {
      return datas.size() * width * height * channel;
    }

    std::vector<const void*> datas;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;
  };

  // Inherits constructor.
  using ImageBuffer::ImageBuffer;

  // This class is neither copyable nor movable.
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;

  void Init(const Info& info);
};

class OffscreenBuffer : public ImageBuffer {
 public:
  OffscreenBuffer(SharedBasicContext context,
                  VkFormat format, VkExtent2D extent);

  // This class is neither copyable nor movable.
  OffscreenBuffer(const OffscreenBuffer&) = delete;
  OffscreenBuffer& operator=(const OffscreenBuffer&) = delete;
};

class DepthStencilBuffer : public ImageBuffer {
 public:
  DepthStencilBuffer(SharedBasicContext context, VkExtent2D extent);

  // This class is neither copyable nor movable.
  DepthStencilBuffer(const DepthStencilBuffer&) = delete;
  DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;

  VkFormat format() const { return format_; }

 private:
  VkFormat format_;
};

class PushConstant {
 public:
  PushConstant() = default;

  // This class is neither copyable nor movable.
  PushConstant(const PushConstant&) = delete;
  PushConstant& operator=(const PushConstant&) = delete;

  ~PushConstant() { delete[] data_; }

  void Init(size_t chunk_size, int num_chunk);

  uint32_t size() const { return size_; }

  template <typename DataType>
  DataType* data(int chunk_index) const {
    return reinterpret_cast<DataType*>(data_ + size_ * chunk_index);
  }

 private:
  uint32_t size_;
  char* data_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H */
