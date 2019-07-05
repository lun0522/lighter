//
//  buffer.h
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H

#include <vector>

#include "absl/types/variant.h"
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

  struct CopyInfos {
    VkDeviceSize total_size;
    std::vector<CopyInfo> copy_infos;
  };

  // This class is neither copyable nor movable.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  virtual ~Buffer() {
    vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
  }

 protected:
  explicit Buffer(SharedBasicContext context) : context_{std::move(context)} {}

  SharedBasicContext context_;
  VkDeviceMemory device_memory_;
};

class DataBuffer : public Buffer {
 public:
  // This class is neither copyable nor movable.
  DataBuffer(const DataBuffer&) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;

  ~DataBuffer() override {
    vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

  VkBuffer buffer_;
};

class VertexBuffer : public DataBuffer {
 public:
  // This class is neither copyable nor movable.
  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

 protected:
  // Inherits constructor.
  using DataBuffer::DataBuffer;

  // For more efficient memory usage, vertices and indices data are put in the
  // same buffer, hence only total size is needed.
  // A dynamic vertex buffer will be visible to host, which can be used for
  // dynamic text rendering. A non-dynamic vertex buffer will be only visible
  // to device, and we will use staging buffers to transfer data to it.
  void CreateBufferAndMemory(VkDeviceSize total_size, bool is_dynamic);
};

class PerVertexBuffer : public VertexBuffer {
 public:
  struct DataInfo {
    // Assuming data is used for one mesh.
    template <typename Container>
    DataInfo(const Container& container)
        : data{container.data()},
          size_per_unit{sizeof(container[0])},
          unit_count{static_cast<int>(container.size())} {}

    // Assuming data is used for multiple meshes, where 'unit_count' of units
    // used for each mesh.
    template <typename Container>
    DataInfo(const Container& container, int unit_count)
        : data{container.data()},
          size_per_unit{sizeof(container[0])},
          unit_count{unit_count} {}

    VkDeviceSize GetTotalSize() const { return size_per_unit * unit_count; }

    const void* data;
    int size_per_unit;
    int unit_count;
  };

  struct InfoNoReuse {
    struct PerMeshInfo {
      DataInfo vertices;
      DataInfo indices;
    };
    std::vector<PerMeshInfo> per_mesh_infos;
  };

  struct InfoReuse {
    int num_mesh;
    DataInfo per_mesh_vertices;
    DataInfo shared_indices;
  };

  using Info = absl::variant<InfoNoReuse, InfoReuse>;

  explicit PerVertexBuffer(SharedBasicContext context)
      : VertexBuffer{std::move(context)} {}

  // This class is neither copyable nor movable.
  PerVertexBuffer(const PerVertexBuffer&) = delete;
  PerVertexBuffer& operator=(const PerVertexBuffer&) = delete;

  virtual void Init(const Info& info);
  void Draw(const VkCommandBuffer& command_buffer,
            int mesh_index, uint32_t instance_count) const;

 protected:
  struct MeshData {
    VkDeviceSize vertices_offset;
    VkDeviceSize indices_offset;
    uint32_t indices_count;
  };

  CopyInfos CreateCopyInfos(const Info& info);
  CopyInfos CreateCopyInfos(const InfoNoReuse& info_no_reuse);
  CopyInfos CreateCopyInfos(const InfoReuse& info_reuse);

  std::vector<MeshData> mesh_datas_;
};

class DynamicPerVertexBuffer : public PerVertexBuffer {
 public:
  // Inherits constructor.
  using PerVertexBuffer::PerVertexBuffer;

  // Reserves space of the given 'size'. If 'size' is less than the current
  // 'buffer_size_', this will be no-op.
  void Reserve(int size);

  // This buffer can be initialized multiple times. If a larger memory is
  // required, the buffer allocated previously will be destructed, and a new one
  // will be allocated. Otherwise, we will reuse the old buffer.
  void Init(const Info& info) override;

 private:
  VkDeviceSize buffer_size_ = 0;
};

class PerInstanceBuffer : public VertexBuffer {
 public:
  explicit PerInstanceBuffer(SharedBasicContext context)
      : VertexBuffer{std::move(context)} {}

  // This class is neither copyable nor movable.
  PerInstanceBuffer(const PerInstanceBuffer&) = delete;
  PerInstanceBuffer& operator=(const PerInstanceBuffer&) = delete;

  void Init(const void* data, size_t data_size);
  void Bind(const VkCommandBuffer& command_buffer,
            uint32_t binding_point) const;
};

class UniformBuffer : public DataBuffer {
 public:
  explicit UniformBuffer(SharedBasicContext context)
      : DataBuffer{std::move(context)} {}

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
  // This class is neither copyable nor movable.
  ImageBuffer(const ImageBuffer&) = delete;
  ImageBuffer& operator=(const ImageBuffer&) = delete;

  ~ImageBuffer() override {
    vkDestroyImage(*context_->device(), image_, context_->allocator());
  }

  const VkImage& image() const { return image_; }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

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

  TextureBuffer(SharedBasicContext context, const Info& info);

  // This class is neither copyable nor movable.
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;
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
