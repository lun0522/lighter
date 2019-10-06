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
#include "third_party/absl/types/variant.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// This is the base class of all buffer classes. The user should use it through
// derived classes. Since all buffers need VkDeviceMemory, which is the handle
// to the data stored in the device memory, it will be held and destroyed by
// this base class, and initialized by derived classes.
class Buffer {
 public:
  // Information we need to copy one chunk of memory from host to device.
  // We assume all the data will be copied to one big chunk of device memory,
  // although they might be stored in different places on the host.
  // We will read data of 'size' bytes starting from 'data' pointer on the host,
  // and then copy to the device memory at 'offset'.
  struct CopyInfo {
    const void* data;
    VkDeviceSize size;
    VkDeviceSize offset;
  };

  // Information we need to copy multiple chunks of memory from host to device.
  struct CopyInfos {
    VkDeviceSize total_size;
    std::vector<CopyInfo> copy_infos;
  };

  // This class is neither copyable nor movable.
  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  virtual ~Buffer() {
    vkFreeMemory(*context_->device(), device_memory_, *context_->allocator());
  }

 protected:
  explicit Buffer(SharedBasicContext context) : context_{std::move(context)} {}

  // Pointer to context.
  const SharedBasicContext context_;

  // Opaque device memory object.
  VkDeviceMemory device_memory_;
};

// This is the base class of the buffers that are used to store one dimensional
// data. The user should use it through derived classes. Since all buffers of
// this kind need VkBuffer, which configures the usage of the data, it will be
// held and destroyed by this base class, and initialized by derived classes.
class DataBuffer : public Buffer {
 public:
  // This class is neither copyable nor movable.
  DataBuffer(const DataBuffer&) = delete;
  DataBuffer& operator=(const DataBuffer&) = delete;

  ~DataBuffer() override {
    vkDestroyBuffer(*context_->device(), buffer_, *context_->allocator());
  }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

  // Opaque buffer object.
  VkBuffer buffer_;
};

// This is the base class of vertex buffers, and provides shared utility
// functions. The user should use it through derived classes.
class VertexBuffer : public DataBuffer {
 public:
  // This class is neither copyable nor movable.
  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

  ~VertexBuffer() override = default;

 protected:
  // Inherits constructor.
  using DataBuffer::DataBuffer;

  // Initializes 'device_memory_' and 'buffer_'.
  // For more efficient memory usage, indices and vertices data are put in the
  // same buffer, hence only total size is needed.
  // If 'is_dynamic' is true, the buffer will be visible to the host, which can
  // be used for dynamic text rendering. Otherwise, the buffer will be only
  // visible to the device, and we will use staging buffers to transfer data.
  void CreateBufferAndMemory(VkDeviceSize total_size, bool is_dynamic);
};

// This is the base class of buffers storing per-vertex data. The user should
// use it through derived classes.
class PerVertexBuffer : public VertexBuffer {
 public:
  // Used to Interpret the data stored in containers.
  struct DataInfo {
    // Assuming the data in 'container' is used for multiple meshes.
    template <typename Container>
    DataInfo(const Container& container, int num_units_per_mesh)
        : data{container.data()},
          num_units_per_mesh{num_units_per_mesh},
          size_per_mesh{sizeof(container[0]) * num_units_per_mesh} {}

    // Assuming all the data in 'container' is used for one mesh.
    template <typename Container>
    explicit DataInfo(const Container& container)
        : DataInfo{container,
                   /*num_units_per_mesh=*/static_cast<int>(container.size())} {}

    const void* data;
    int num_units_per_mesh;
    size_t size_per_mesh;
  };

  // Holds data information for multiple meshes that share indices.
  // Each mesh has the same number of vertices.
  struct ShareIndicesDataInfo {
    int num_meshes;
    DataInfo per_mesh_vertices;
    DataInfo shared_indices;
  };

  // Holds data information for multiple meshes that do not share indices.
  // Each mesh may have different number of indices and vertices.
  struct NoShareIndicesDataInfo {
    // Holds data information for each mesh.
    struct PerMeshInfo {
      DataInfo indices;
      DataInfo vertices;
    };
    std::vector<PerMeshInfo> per_mesh_infos;
  };

  using BufferDataInfo = absl::variant<ShareIndicesDataInfo,
                                       NoShareIndicesDataInfo>;

  // This class is neither copyable nor movable.
  PerVertexBuffer(const PerVertexBuffer&) = delete;
  PerVertexBuffer& operator=(const PerVertexBuffer&) = delete;

  // Renders one mesh with 'mesh_index' for 'instance_count' times.
  void Draw(const VkCommandBuffer& command_buffer,
            int mesh_index, uint32_t instance_count) const;

 protected:
  // Inherits constructor.
  using VertexBuffer::VertexBuffer;

  // Holds the number of indices in the mesh and the data size offset within the
  // vertex buffer.
  struct MeshDataInfo {
    uint32_t indices_count;
    VkDeviceSize indices_offset;
    VkDeviceSize vertices_offset;
  };

  // These functions populate 'mesh_data_infos_' and return instances of
  // 'CopyInfos' that can be used for copying data from the host to device.
  // Previous content in 'mesh_data_infos_' will be cleared.
  CopyInfos CreateCopyInfos(const BufferDataInfo& info);
  CopyInfos CreateCopyInfos(const ShareIndicesDataInfo& info_no_reuse);
  CopyInfos CreateCopyInfos(const NoShareIndicesDataInfo& info_reuse);

  // Holds data information for all meshes stored in the vertex buffer.
  std::vector<MeshDataInfo> mesh_data_infos_;
};

// This class creates a vertex buffer that stores static data, which will be
// transferred to the device via the staging buffer.
class StaticPerVertexBuffer : public PerVertexBuffer {
 public:
  StaticPerVertexBuffer(SharedBasicContext context, const BufferDataInfo& info);

  // This class is neither copyable nor movable.
  StaticPerVertexBuffer(const StaticPerVertexBuffer&) = delete;
  StaticPerVertexBuffer& operator=(const StaticPerVertexBuffer&) = delete;
};

// This class creates a vertex buffer that stores dynamic data, and is able to
// re-allocate a larger buffer internally if necessary. This is useful when
// we want to render the text whose length may change.
// If possible, the user should compute the maximum required space of this
// buffer, and pass it as 'initial_size' to the constructor, so that we can
// pre-allocate enough space and always reuse the same chunk of device memory.
// Since the data is dynamic, this buffer will be visible to the host, and we
// don't use the staging buffer.
class DynamicPerVertexBuffer : public PerVertexBuffer {
 public:
  // 'initial_size' should be greater than 0.
  DynamicPerVertexBuffer(SharedBasicContext context, int initial_size)
      : PerVertexBuffer(std::move(context)) {
    Reserve(initial_size);
  }

  // This class is neither copyable nor movable.
  DynamicPerVertexBuffer(const DynamicPerVertexBuffer&) = delete;
  DynamicPerVertexBuffer& operator=(const DynamicPerVertexBuffer&) = delete;

  // This buffer can be re-allocated multiple times. If a larger memory is
  // required, the buffer allocated previously will be destructed, and a new one
  // will be allocated. Otherwise, we will reuse the old buffer.
  void Allocate(const BufferDataInfo& info);

 private:
  // Reserves space of the given 'size'. If 'size' is less than the current
  // 'buffer_size_', this will be no-op.
  void Reserve(int size);

  // Tracks the current size of buffer. This is initialized to be 0 so that we
  // will not try to deallocate an uninitialized buffer in 'Reserve()'.
  VkDeviceSize buffer_size_ = 0;
};

// Holds vertex data of all instances.
// Data will be copied to device via the staging buffer.
class PerInstanceBuffer : public VertexBuffer {
 public:
  PerInstanceBuffer(SharedBasicContext context,
                    const void* data, size_t data_size);

  // This class is neither copyable nor movable.
  PerInstanceBuffer(const PerInstanceBuffer&) = delete;
  PerInstanceBuffer& operator=(const PerInstanceBuffer&) = delete;

  // Binds vertex data to the given 'binding_point'.
  void Bind(const VkCommandBuffer& command_buffer,
            uint32_t binding_point) const;
};

// Holds uniform buffer data on both the host and device. To make it more
// flexible, the user may allocate several chunks of memory in this buffer.
// For rendering a single frame (for example, when we render single characters
// onto a large text texture), these chunks may store data for different meshes.
// For rendering multiple frames (which is more general), all chunks can be used
// for one mesh, and each chunk used for one frame.
// The user should use 'HostData()' to update the data on the host, and then
// call 'Flush()' to send it to the device. This buffer will be visible to the
// host, and we don't use the staging buffer.
class UniformBuffer : public DataBuffer {
 public:
  UniformBuffer(SharedBasicContext context, size_t chunk_size, int num_chunks);

  // This class is neither copyable nor movable.
  UniformBuffer(const UniformBuffer&) = delete;
  UniformBuffer& operator=(const UniformBuffer&) = delete;

  ~UniformBuffer() override { delete data_; }

  // Returns a pointer to the data on the host, casted to 'DataType'.
  template <typename DataType>
  DataType* HostData(int chunk_index) const {
    return reinterpret_cast<DataType*>(data_ + chunk_data_size_ * chunk_index);
  }

  // Flushes the data from host to device.
  void Flush(int chunk_index) const;

  // Returns the description of the data chunk at 'chunk_index'.
  VkDescriptorBufferInfo GetDescriptorInfo(int chunk_index) const;

 private:
  // Pointer to data on the host.
  char* data_;

  // Size of each chunk of data in bytes on the host.
  size_t chunk_data_size_;

  // Since we have to align the memory offset with
  // 'minUniformBufferOffsetAlignment' on the device, this stores the aligned
  // value of 'chunk_data_size_'.
  size_t chunk_memory_size_;
};

// This is the base class of buffers storing images. The user should use it
// through derived classes. Since all buffers of this kind need VkImage,
// which configures how do we use the device memory to store multidimensional
// data, it will be held and destroyed by this base class, and initialized by
// derived classes.
class ImageBuffer : public Buffer {
 public:
  // This class is neither copyable nor movable.
  ImageBuffer(const ImageBuffer&) = delete;
  ImageBuffer& operator=(const ImageBuffer&) = delete;

  ~ImageBuffer() override {
    vkDestroyImage(*context_->device(), image_, *context_->allocator());
  }

  // Accessors.
  const VkImage& image() const { return image_; }

 protected:
  // Inherits constructor.
  using Buffer::Buffer;

  // Opaque image object.
  VkImage image_;
};

// This class copies an image on the host to device via the staging buffer,
// and generates mipmaps if requested.
class TextureBuffer : public ImageBuffer {
 public:
  // Description of the image data. The size of 'datas' can only be either 1
  // or 6 (for cubemaps), otherwise, the constructor will throw an exception.
  struct Info{
    VkExtent2D GetExtent2D() const { return {width, height}; }
    VkExtent3D GetExtent3D() const { return {width, height, /*depth=*/1}; }
    VkDeviceSize GetDataSize() const {
      return datas.size() * width * height * channel;
    }

    std::vector<const void*> datas;
    VkFormat format;
    uint32_t width;
    uint32_t height;
    uint32_t channel;
  };

  TextureBuffer(SharedBasicContext context,
                bool generate_mipmaps, const Info& info);

  // This class is neither copyable nor movable.
  TextureBuffer(const TextureBuffer&) = delete;
  TextureBuffer& operator=(const TextureBuffer&) = delete;

  // Accessors.
  uint32_t mip_levels() const { return mip_levels_; }

 private:
  // Level of mipmaps.
  uint32_t mip_levels_;
};

// This class creates an image buffer that can be used as offscreen rendering
// target. No data transfer is required at construction.
class OffscreenBuffer : public ImageBuffer {
 public:
  OffscreenBuffer(SharedBasicContext context,
                  const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  OffscreenBuffer(const OffscreenBuffer&) = delete;
  OffscreenBuffer& operator=(const OffscreenBuffer&) = delete;
};

// This class creates an image buffer that can be used as depth stencil image
// buffer. No data transfer is required at construction.
class DepthStencilBuffer : public ImageBuffer {
 public:
  DepthStencilBuffer(SharedBasicContext context,
                     const VkExtent2D& extent, VkFormat format);

  // This class is neither copyable nor movable.
  DepthStencilBuffer(const DepthStencilBuffer&) = delete;
  DepthStencilBuffer& operator=(const DepthStencilBuffer&) = delete;
};

// This class creates an image buffer for multisampling. No data transfer is
// required at construction.
class MultisampleBuffer : public ImageBuffer {
 public:
  enum class Type { kColor, kDepthStencil };

  MultisampleBuffer(SharedBasicContext context,
                    Type type, const VkExtent2D& extent, VkFormat format,
                    VkSampleCountFlagBits sample_count);

  // This class is neither copyable nor movable.
  MultisampleBuffer(const MultisampleBuffer&) = delete;
  MultisampleBuffer& operator=(const MultisampleBuffer&) = delete;
};

// Holds a small amount of data that can be modified per-frame efficiently.
// To make it flexible, the user may use one chunk of memory for each frame,
// just like the uniform buffer. What is different is that this data does not
// need alignment, and the total size is very limited. According to the Vulkan
// specification, to make it compatible with all devices, we only allow the user
// to push at most 128 bytes per-frame.
// The user should use 'HostData()' to update the data on the host, and then
// call 'Flush()' to send it to the device.
class PushConstant {
 public:
  // 'size_per_frame' must be less than 128.
  PushConstant(const SharedBasicContext& context,
               size_t size_per_frame, int num_frames);

  // This class is neither copyable nor movable.
  PushConstant(const PushConstant&) = delete;
  PushConstant& operator=(const PushConstant&) = delete;

  ~PushConstant() { delete[] data_; }

  // Returns a pointer to the data on the host, casted to 'DataType'.
  template <typename DataType>
  DataType* HostData(int frame) const {
    return reinterpret_cast<DataType*>(data_ + size_per_frame_ * frame);
  }

  // Flushes the data from host to device. Data of 'size_per_frame_' will be
  // sent, and written to the target with 'target_offset' bytes.
  void Flush(const VkCommandBuffer& command_buffer,
             const VkPipelineLayout& pipeline_layout,
             int frame, uint32_t target_offset,
             VkShaderStageFlags shader_stage) const;

  // Accessors.
  uint32_t size_per_frame() const { return size_per_frame_; }

 private:
  // Pointer to data on the host.
  char* data_;

  // Size of data for one frame in bytes.
  uint32_t size_per_frame_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_BUFFER_H */
