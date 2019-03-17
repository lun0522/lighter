//
//  buffer.cc
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "buffer.h"

#include <array>

#include "context.h"
#include "command.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::array;
using std::vector;

uint32_t FindMemoryType(SharedContext context,
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
      if ((flags & mem_properties) == mem_properties)  // has required property
        return i;
    }
  }
  throw std::runtime_error{"Failed to find suitable memory type"};
}

VkBuffer CreateBuffer(SharedContext context,
                      VkDeviceSize data_size,
                      VkBufferUsageFlags buffer_usage) {
  // create buffer
  VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
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

VkDeviceMemory CreateBufferMemory(SharedContext context,
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

VkImage CreateImage(SharedContext context,
                    VkFormat format,
                    VkExtent3D extent,
                    VkImageLayout layout,
                    VkImageUsageFlags usage) {
  VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/NULL_FLAG,
      /*imageType=*/VK_IMAGE_TYPE_2D,
      format,
      extent,
      /*mipLevels=*/1,
      /*arrayLayers=*/1,
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

VkDeviceMemory CreateImageMemory(SharedContext context,
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

void TransitionImageLayout(SharedContext context,
                           const VkImage& image,
                           array<VkImageLayout, 2> image_layouts,
                           array<VkAccessFlags, 2> barrier_access_flags,
                           array<VkPipelineStageFlags, 2> pipeline_stages) {
  const Queues::Queue transfer_queue = context->queues().transfer;

  // one-time transition command
  command::OneTimeCommand(context, transfer_queue,
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
                /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
                /*baseMipLevel=*/0,
                /*levelCount=*/1,
                /*baseArrayLayer=*/0,
                /*layerCount=*/1,
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

struct HostToBufferCopyInfo {
  const void* data;
  VkDeviceSize size;
  VkDeviceSize offset;
};

void CopyHostToBuffer(SharedContext context,
                      VkDeviceSize map_size,
                      VkDeviceSize map_offset,
                      const VkDeviceMemory& device_memory,
                      const vector<HostToBufferCopyInfo>& copy_infos) {
  // data transfer may not happen immediately, for example because it is only
  // written to cache and not yet to device. we can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // specify VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient)
  void* dst;
  vkMapMemory(*context->device(), device_memory, map_offset, map_size, 0, &dst);
  for (const auto& info : copy_infos)
    memcpy(static_cast<char*>(dst) + info.offset, info.data, info.size);
  vkUnmapMemory(*context->device(), device_memory);
}

void CopyBufferToBuffer(SharedContext context,
                        VkDeviceSize data_size,
                        const VkBuffer& src_buffer,
                        const VkBuffer& dst_buffer) {
  // one-time copy command
  command::OneTimeCommand(context, context->queues().transfer,
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

void CopyBufferToImage(SharedContext context,
                       const VkBuffer& buffer,
                       const VkImage& image,
                       const VkExtent3D& image_extent,
                       VkImageLayout image_layout) {
  // one-time copy command
  command::OneTimeCommand(context, context->queues().transfer,
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
                /*layerCount=*/1,
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

void VertexBuffer::Init(SharedContext context,
                        const buffer::DataInfo& vertex_info,
                        const buffer::DataInfo& index_info) {
  context_ = context;

  VkDeviceSize total_size = vertex_info.data_size + index_info.data_size;
  index_offset_ = vertex_info.data_size;
  index_count_  = index_info.unit_count;

  // vertex/index buffer cannot be most efficient if it has to be visible to
  // both host and device, so we create vertex/index buffer that is only visible
  // to device, and staging buffer that is visible to both and transfers data
  // to vertex/index buffer
  VkBuffer staging_buffer = CreateBuffer(           // source of transfer
      context_, total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context_, staging_buffer,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT           // host can access it
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);  // see host cache management

  // copy from host to staging buffer
  CopyHostToBuffer(context_, total_size, 0, staging_memory, {
      {vertex_info.data, vertex_info.data_size, /*offset=*/0},
      { index_info.data,  index_info.data_size, /*offset=*/index_offset_},
  });

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

void VertexBuffer::Draw(const VkCommandBuffer& command_buffer) const {
  static const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer_, &offset);
  vkCmdBindIndexBuffer(command_buffer, buffer_, index_offset_,
                       VK_INDEX_TYPE_UINT32);
  // (index_count, instance_count, first_index, vertex_offset, first_instance)
  vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
}

VertexBuffer::~VertexBuffer() {
  vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

void UniformBuffer::Init(SharedContext context,
                         const buffer::ChunkInfo& chunk_info,
                         uint32_t binding_point,
                         VkShaderStageFlags shader_stage) {
  context_ = context;

  data_ = static_cast<const char*>(chunk_info.data);
  // offset is required to be multiple of minUniformBufferOffsetAlignment
  // which is why we have actual data size |chunk_data_size_| and its
  // aligned size |chunk_memory_size_|
  VkDeviceSize alignment =
      context_->physical_device().limits().minUniformBufferOffsetAlignment;
  chunk_data_size_ = chunk_info.chunk_size;
  chunk_memory_size_ = (chunk_data_size_ + alignment) / alignment * alignment;

  buffer_ = CreateBuffer(context_, chunk_info.num_chunk * chunk_memory_size_,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  device_memory_ = CreateBufferMemory(
      context_, buffer_,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  // create descriptor so that shader knows how to interpret data in buffer
  vector<uint32_t> binding_points(chunk_info.num_chunk, binding_point);
  descriptor_.Init(context_, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, binding_points,
                   shader_stage);

  vector<VkDescriptorBufferInfo> buffer_infos(binding_points.size());
  for (size_t i = 0; i < chunk_info.num_chunk; ++i) {
    buffer_infos[i] = VkDescriptorBufferInfo{
        buffer_,
        /*offset=*/chunk_memory_size_ * i,
        /*range=*/chunk_data_size_,
    };
  }
  descriptor_.UpdateBufferInfos(buffer_infos);
}

void UniformBuffer::Update(size_t chunk_index) const {
  VkDeviceSize src_offset = chunk_data_size_ * chunk_index;
  VkDeviceSize dst_offset = chunk_memory_size_ * chunk_index;
  CopyHostToBuffer(
      context_, chunk_data_size_, dst_offset, device_memory_,
      {{data_ + src_offset, chunk_data_size_, /*offset=*/0}});
}

void UniformBuffer::Bind(const VkCommandBuffer& command_buffer,
                         const VkPipelineLayout& pipeline_layout,
                         size_t chunk_index) const {
  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
      0, 1, &descriptor_.sets()[chunk_index], 0, nullptr);
}

UniformBuffer::~UniformBuffer() {
  vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

void ImageBuffer::Init(SharedContext context,
                       const buffer::ImageInfo& image_info) {
  context_ = context;

  VkExtent3D image_extent = image_info.extent();
  VkDeviceSize data_size = image_info.data_size();

  // create staging buffer and associated memory
  VkBuffer staging_buffer = CreateBuffer(           // source of transfer
      context_, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context_, staging_buffer,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT           // host can access it
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);  // see host cache management

  // copy from host to staging buffer
  CopyHostToBuffer(context_, data_size, 0, staging_memory, {
      {image_info.data, data_size, /*offset=*/0},
  });

  // create final image buffer
  image_ = CreateImage(context_, image_info.format, image_extent,
                       VK_IMAGE_LAYOUT_UNDEFINED,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // copy data from staging buffer to image buffer. we need to do some
  // transitions so that image buffer is eventually only visible to device
  TransitionImageLayout(
      context_, image_,
      {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
      {VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT},
      {VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT});
  CopyBufferToImage(context_, staging_buffer, image_, image_extent,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  TransitionImageLayout(
      context_, image_,
      {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
      {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT});

  // cleanup transient objects
  vkDestroyBuffer(*context_->device(), staging_buffer, context_->allocator());
  vkFreeMemory(*context_->device(), staging_memory, context_->allocator());
}

ImageBuffer::~ImageBuffer() {
  vkDestroyImage(*context_->device(), image_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

} /* namespace vulkan */
} /* namespace wrapper */
