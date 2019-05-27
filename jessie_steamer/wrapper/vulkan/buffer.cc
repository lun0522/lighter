//
//  buffer.cc
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/buffer.h"

#include <array>
#include <cstring>
#include <stdexcept>

#include "absl/strings/str_format.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/context.h"
#include "jessie_steamer/wrapper/vulkan/macro.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::array;
using std::runtime_error;
using std::vector;

uint32_t FindMemoryType(const SharedContext& context,
                        uint32_t type_filter,
                        VkMemoryPropertyFlags mem_properties) {
  // query available types of memory
  //   .memoryHeaps: memory heaps from which memory can be allocated
  //   .memoryTypes: memory types that can be used to access memory allocated
  //                 from heaps
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(*context->physical_device(), &properties);

  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if (type_filter & (1 << i)) {  // type is suitable for buffer
      auto flags = properties.memoryTypes[i].propertyFlags;
      if ((flags & mem_properties) == mem_properties) { // has required property
        return i;
      }
    }
  }
  throw runtime_error{"Failed to find suitable memory type"};
}

VkFormat FindImageFormat(const SharedContext& context,
                         const vector<VkFormat>& candidates,
                         VkFormatFeatureFlags features) {
  for (auto format : candidates) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(*context->physical_device(),
                                        format, &properties);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  throw runtime_error{"Failed to find suitable image type"};
}

VkBuffer CreateBuffer(const SharedContext& context,
                      VkDeviceSize data_size,
                      VkBufferUsageFlags buffer_usage) {
  // create buffer
  VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      data_size,
      buffer_usage,
      /*sharingMode=*/VK_SHARING_MODE_EXCLUSIVE,  // only graphics queue access
      /*queueFamilyIndexCount=*/0,
      /*pQueueFamilyIndices=*/nullptr,
  };

  VkBuffer buffer;
  ASSERT_SUCCESS(vkCreateBuffer(*context->device(), &buffer_info,
                                context->allocator(), &buffer),
                 "Failed to create buffer");
  return buffer;
}

VkDeviceMemory CreateBufferMemory(const SharedContext& context,
                                  const VkBuffer& buffer,
                                  VkMemoryPropertyFlags mem_properties) {
  const VkDevice& device = *context->device();

  // query memory requirements for this buffer
  //   .size: size of required amount of memory
  //   .alignment: offset where this buffer begins in allocated region
  //   .memoryTypeBits: memory type suitable for this buffer
  VkMemoryRequirements mem_requirements;
  vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

  // allocate memory on device
  VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/mem_requirements.size,
      /*memoryTypeIndex=*/FindMemoryType(
          context, mem_requirements.memoryTypeBits, mem_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(
      vkAllocateMemory(device, &memory_info, context->allocator(), &memory),
      "Failed to allocate buffer memory");

  // associate allocated memory with buffer
  // since this memory is specifically allocated for this buffer, the last
  // parameter |memoryOffset| is simply 0
  // otherwise it should be selected according to mem_requirements.alignment
  vkBindBufferMemory(device, buffer, memory, 0);

  return memory;
}

VkImage CreateImage(const SharedContext& context,
                    VkImageCreateFlags flags,
                    VkFormat format,
                    VkExtent3D extent,
                    VkImageUsageFlags usage,
                    uint32_t layer_count) {
  VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      flags,
      /*imageType=*/VK_IMAGE_TYPE_2D,
      format,
      extent,
      /*mipLevels=*/1,
      layer_count,
      /*samples=*/VK_SAMPLE_COUNT_1_BIT,
      // use VK_IMAGE_TILING_LINEAR if we want to directly access texels of
      // image, otherwise use VK_IMAGE_TILING_OPTIMAL for optimal layout
      /*tiling=*/VK_IMAGE_TILING_OPTIMAL,
      usage,
      /*sharingMode=*/VK_SHARING_MODE_EXCLUSIVE,
      /*queueFamilyIndexCount=*/0,
      /*pQueueFamilyIndices=*/nullptr,
      // can only be VK_IMAGE_LAYOUT_UNDEFINED or VK_IMAGE_LAYOUT_PREINITIALIZED
      // the first one discards texels while the latter one preserves texels, so
      // the latter one can be used with VK_IMAGE_TILING_LINEAR
      /*initialLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage image;
  ASSERT_SUCCESS(vkCreateImage(*context->device(), &image_info,
                               context->allocator(), &image),
                 "Failed to create image");
  return image;
}

VkDeviceMemory CreateImageMemory(const SharedContext& context,
                                 const VkImage& image,
                                 VkMemoryPropertyFlags mem_properties) {
  const VkDevice& device = *context->device();

  // query memory requirements for this image
  VkMemoryRequirements mem_requirements;
  vkGetImageMemoryRequirements(device, image, &mem_requirements);

  // allocate memory on device
  VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/mem_requirements.size,
      /*memoryTypeIndex=*/FindMemoryType(
          context, mem_requirements.memoryTypeBits, mem_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(
      vkAllocateMemory(device, &memory_info, context->allocator(), &memory),
      "Failed to allocate image memory");
  vkBindImageMemory(device, image, memory, 0);
  return memory;
}

void TransitionImageLayout(const SharedContext& context,
                           const VkImage& image,
                           VkImageAspectFlags image_aspect_mask,
                           array<VkImageLayout, 2> image_layouts,
                           array<VkAccessFlags, 2> barrier_access_flags,
                           array<VkPipelineStageFlags, 2> pipeline_stages,
                           uint32_t layer_count) {
  const Queues::Queue transfer_queue = context->queues().transfer;

  // one-time transition command
  Command::OneTimeCommand(context, transfer_queue,
                          [&](const VkCommandBuffer& command_buffer) {
        VkImageMemoryBarrier barrier{
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            /*pNext=*/nullptr,
            barrier_access_flags[0],  // operations before barrier
            barrier_access_flags[1],  // operations waiting on barrier
            image_layouts[0],
            image_layouts[1],
            /*srcQueueFamilyIndex=*/transfer_queue.family_index,
            /*dstQueueFamilyIndex=*/transfer_queue.family_index,
            image,
            // specify which part of image to use
            VkImageSubresourceRange{
                /*aspectMask=*/image_aspect_mask,
                /*baseMipLevel=*/0,
                /*levelCount=*/1,
                /*baseArrayLayer=*/0,
                layer_count,
            },
        };

        // wait for barrier
        vkCmdPipelineBarrier(
            command_buffer,
            // operations before barrier should occur in which pipeline stage
            pipeline_stages[0],
            // operations waiting on barrier should occur in which stage
            pipeline_stages[1],
            // either 0 or VK_DEPENDENCY_BY_REGION_BIT. the latter one allows
            // reading from regions that have been written, even if entire
            // writing has not yet finished
            /*dependencyFlags=*/0,
            /*memoryBarrierCount=*/0,
            /*pMemoryBarriers=*/nullptr,
            /*bufferMemoryBarrierCount=*/0,
            /*pBufferMemoryBarriers=*/nullptr,
            /*imageMemoryBarrierCount=*/1,
            &barrier);
      }
  );
}

void CopyHostToBuffer(const SharedContext& context,
                      VkDeviceSize map_size,
                      VkDeviceSize map_offset,
                      const VkDeviceMemory& device_memory,
                      const vector<buffer::CopyInfo>& copy_infos) {
  // data transfer may not happen immediately, for example because it is only
  // written to cache and not yet to device. we can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // specify VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient)
  void* dst;
  vkMapMemory(*context->device(), device_memory, map_offset, map_size, 0, &dst);
  for (const auto& info : copy_infos) {
    std::memcpy(static_cast<char*>(dst) + info.offset, info.data, info.size);
  }
  vkUnmapMemory(*context->device(), device_memory);
}

void CopyBufferToBuffer(const SharedContext& context,
                        VkDeviceSize data_size,
                        const VkBuffer& src_buffer,
                        const VkBuffer& dst_buffer) {
  // one-time copy command
  Command::OneTimeCommand(context, context->queues().transfer,
                          [&](const VkCommandBuffer& command_buffer) {
        VkBufferCopy region{
            /*srcOffset=*/0,
            /*dstOffset=*/0,
            data_size,
        };
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &region);
      }
  );
}

void CopyBufferToImage(const SharedContext& context,
                       const VkBuffer& buffer,
                       const VkImage& image,
                       const VkExtent3D& image_extent,
                       VkImageLayout image_layout,
                       uint32_t layer_count) {
  // one-time copy command
  Command::OneTimeCommand(context, context->queues().transfer,
                          [&](const VkCommandBuffer& command_buffer) {
        VkBufferImageCopy region{
            // first three parameters specify pixels layout in buffer
            // setting all of them to 0 means pixels are tightly packed
            /*bufferOffset=*/0,
            /*bufferRowLength=*/0,
            /*bufferImageHeight=*/0,
            VkImageSubresourceLayers{
                /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
                /*mipLevel=*/0,
                /*baseArrayLayer=*/0,
                layer_count,
            },
            /*imageOffset=*/{0, 0, 0},
            image_extent,
        };
        vkCmdCopyBufferToImage(
            command_buffer, buffer, image, image_layout, 1, &region);
      }
  );
}

} /* namespace */

void VertexBuffer::CopyHostData(const vector<buffer::CopyInfo>& copy_infos,
                                size_t total_size) {
  // create staging buffer and associated memory
  VkBuffer staging_buffer = CreateBuffer(           // source of transfer
      context_, total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context_, staging_buffer,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT           // host can access it
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);  // see host cache management

  // copy from host to staging buffer
  CopyHostToBuffer(context_, total_size, 0, staging_memory, copy_infos);

  // create final buffer that is only visible to device
  // for more efficient memory usage, we put vertex and index data in one buffer
  buffer_ = CreateBuffer(context_, total_size,    // destination of transfer
                         VK_BUFFER_USAGE_TRANSFER_DST_BIT
                             | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                             | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
  device_memory_ = CreateBufferMemory(            // only accessible for device
      context_, buffer_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // copy from staging buffer to final buffer
  // graphics or compute queues implicitly have transfer capability
  CopyBufferToBuffer(context_, total_size, staging_buffer, buffer_);

  // cleanup transient objects
  vkDestroyBuffer(*context_->device(), staging_buffer, context_->allocator());
  vkFreeMemory(*context_->device(), staging_memory, context_->allocator());
}

void PerVertexBuffer::Init(const SharedContext& context,
                           const vector<PerVertexBuffer::Info>& infos) {
  context_ = context;

  mesh_datas_.reserve(infos.size());
  vector<buffer::CopyInfo> copy_infos;
  copy_infos.reserve(infos.size() * 2);
  VkDeviceSize total_size = 0;
  for (const auto& info : infos) {
    mesh_datas_.emplace_back(MeshData{
        total_size,
        total_size + info.vertices.data_size,
        info.indices.unit_count,
    });
    copy_infos.emplace_back(buffer::CopyInfo{
        info.vertices.data,
        info.vertices.data_size,
        total_size,
    });
    copy_infos.emplace_back(buffer::CopyInfo{
        info.indices.data,
        info.indices.data_size,
        total_size + info.vertices.data_size,
    });
    total_size += info.vertices.data_size + info.indices.data_size;
  }
  CopyHostData(copy_infos, total_size);
}

void PerVertexBuffer::Draw(const VkCommandBuffer& command_buffer,
                           int mesh_index,
                           uint32_t instance_count) const {
  const MeshData& data = mesh_datas_[mesh_index];
  vkCmdBindVertexBuffers(command_buffer, buffer::kPerVertexBindingPoint,
                         /*bindingCount=*/1, &buffer_, &data.vertices_offset);
  vkCmdBindIndexBuffer(command_buffer, buffer_, data.indices_offset,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(command_buffer, data.indices_count, instance_count,
                   /*firstIndex=*/0, /*vertexOffset=*/0, /*firstInstance=*/0);
}

void PerInstanceBuffer::Init(const SharedContext& context,
                             const void* data,
                             size_t data_size) {
  context_ = context;
  CopyHostData({{data, data_size, /*offset=*/0}}, data_size);
}

void PerInstanceBuffer::Bind(const VkCommandBuffer& command_buffer) {
  static constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, buffer::kPerInstanceBindingPoint,
                         /*bindingCount=*/1, &buffer_, &offset);
}

VertexBuffer::~VertexBuffer() {
  vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

void UniformBuffer::Init(const SharedContext& context,
                         size_t chunk_size,
                         int num_chunk) {
  context_ = context;

  // offset is required to be multiple of minUniformBufferOffsetAlignment
  // which is why we have actual data size |chunk_data_size_| and its
  // aligned size |chunk_memory_size_|
  VkDeviceSize alignment =
      context_->physical_device().limits().minUniformBufferOffsetAlignment;
  chunk_data_size_ = chunk_size;
  chunk_memory_size_ =
      (chunk_data_size_ + alignment - 1) / alignment * alignment;

  data_ = new char[chunk_data_size_ * num_chunk];
  buffer_ = CreateBuffer(context_, chunk_memory_size_ * num_chunk,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  device_memory_ = CreateBufferMemory(
      context_, buffer_,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void UniformBuffer::Flush(int chunk_index) const {
  VkDeviceSize src_offset = chunk_data_size_ * chunk_index;
  VkDeviceSize dst_offset = chunk_memory_size_ * chunk_index;
  CopyHostToBuffer(
      context_, chunk_data_size_, dst_offset, device_memory_,
      {{data_ + src_offset, chunk_data_size_, /*offset=*/0}});
}

UniformBuffer::~UniformBuffer() {
  delete data_;
  vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

VkDescriptorBufferInfo UniformBuffer::descriptor_info(
    int chunk_index) const {
  return VkDescriptorBufferInfo{
      buffer_,
      /*offset=*/chunk_memory_size_ * chunk_index,
      /*range=*/chunk_data_size_,
  };
}

void TextureBuffer::Init(const SharedContext& context,
                         const Info& info) {
  context_ = context;

  VkExtent3D image_extent = info.extent();
  VkDeviceSize data_size = info.data_size();

  auto layer_count = CONTAINER_SIZE(info.datas);
  if (layer_count != 1 && layer_count != buffer::kCubemapImageCount) {
    throw runtime_error{absl::StrFormat(
        "Wrong number of images: %d", layer_count)};
  }

  // create staging buffer and associated memory
  VkBuffer staging_buffer = CreateBuffer(           // source of transfer
      context_, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context_, staging_buffer,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT           // host can access it
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);  // see host cache management

  // copy from host to staging buffer
  VkDeviceSize image_size = data_size / layer_count;
  for (int i = 0; i < layer_count; ++i) {
    CopyHostToBuffer(context_, image_size, image_size * i, staging_memory, {
        {info.datas[i], image_size, /*offset=*/0},
    });
  }

  // create final image buffer
  VkImageCreateFlags flags = info.datas.size() == buffer::kCubemapImageCount
                                 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT
                                 : nullflag;
  image_ = CreateImage(context_, flags, info.format, image_extent,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT, layer_count);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // copy data from staging buffer to image buffer
  TransitionImageLayout(
      context_, image_, VK_IMAGE_ASPECT_COLOR_BIT,
      {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
      {VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT},
      {VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
      layer_count);
  CopyBufferToImage(context_, staging_buffer, image_, image_extent,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer_count);
  TransitionImageLayout(
      context_, image_, VK_IMAGE_ASPECT_COLOR_BIT,
      {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
      {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
      layer_count);

  // cleanup transient objects
  vkDestroyBuffer(*context_->device(), staging_buffer, context_->allocator());
  vkFreeMemory(*context_->device(), staging_memory, context_->allocator());
}

TextureBuffer::~TextureBuffer() {
  vkDestroyImage(*context_->device(), image_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

void DepthStencilBuffer::Init(const SharedContext& context,
                              VkExtent2D extent) {
  context_ = context;

  // no need to send any data to buffer
  format_ = FindImageFormat(
      context_, {VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT},
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  image_ = CreateImage(context_, nullflag, format_,
                       {extent.width, extent.height, /*depth=*/1},
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       /*layer_count=*/1);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  TransitionImageLayout(context_, image_,
                        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
                        {VK_IMAGE_LAYOUT_UNDEFINED,
                         VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL},
                        {VK_ACCESS_HOST_WRITE_BIT,
                         VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                             | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT},
                        {VK_PIPELINE_STAGE_HOST_BIT,
                         VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT},
                        /*layer_count=*/1);
}

void DepthStencilBuffer::Cleanup() {
  vkDestroyImage(*context_->device(), image_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

void PushConstant::Init(size_t chunk_size, int num_chunk) {
  if (chunk_size > buffer::kMaxPushConstantSize) {
    throw runtime_error{absl::StrFormat(
        "Pushing constant of size %d bytes. To be compatible with all devices, "
        "the size should NOT be greater than %d bytes.",
        chunk_size, buffer::kMaxPushConstantSize)};
  }
  size_ = static_cast<uint32_t>(chunk_size);
  data_ = new char[size_ * num_chunk];
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
