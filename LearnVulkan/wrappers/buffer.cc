//
//  buffer.cc
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "buffer.h"

#include "application.h"
#include "command.h"

namespace vulkan {
namespace wrapper {

namespace {

uint32_t FindMemoryType(uint32_t type_filter,
                        VkMemoryPropertyFlags mem_properties,
                        const VkPhysicalDevice& physical_device) {
  // query available types of memory
  //   .memoryHeaps: memory heaps from which memory can be allocated
  //   .memoryTypes: memory types that can be used to access memory allocated
  //                 from heaps
  VkPhysicalDeviceMemoryProperties properties{};
  vkGetPhysicalDeviceMemoryProperties(physical_device, &properties);

  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if (type_filter & (1 << i)) { // type is suitable for buffer
      auto flags = properties.memoryTypes[i].propertyFlags;
      if ((flags & mem_properties) == mem_properties) // has required properties
        return i;
    }
  }
  throw std::runtime_error{"Failed to find suitable memory type"};
}

VkBuffer CreateBuffer(VkBufferUsageFlags buffer_usage,
                      VkDeviceSize data_size,
                      const VkDevice& device) {
  // create buffer
  VkBufferCreateInfo buffer_info{
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = data_size,
    .usage = buffer_usage,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE, // only graphics queue will access
  };

  VkBuffer buffer{};
  ASSERT_SUCCESS(vkCreateBuffer(device, &buffer_info, nullptr, &buffer),
                 "Failed to create buffer");
  return buffer;
}

VkDeviceMemory CreateBufferMemory(VkMemoryPropertyFlags mem_properties,
                                  const VkBuffer& buffer,
                                  const VkDevice& device,
                                  const VkPhysicalDevice& physical_device) {
  // query memory requirements for this buffer
  //   .size: size of required amount of memory
  //   .alignment: offset where this buffer begins in allocated region
  //   .memoryTypeBits: memory type suitable for this buffer
  VkMemoryRequirements mem_requirements{};
  vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);

  // allocate memory on device
  VkMemoryAllocateInfo memory_info{
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_requirements.size,
    .memoryTypeIndex = FindMemoryType(
        mem_requirements.memoryTypeBits, mem_properties, physical_device),
  };

  VkDeviceMemory memory{};
  ASSERT_SUCCESS(vkAllocateMemory(device, &memory_info, nullptr, &memory),
                 "Failed to allocate buffer memory");

  // associate allocated memory with buffer
  // since this memory is specifically allocated for this buffer, the last
  // parameter |memoryOffset| is simply 0
  // otherwise it should be selected according to mem_requirements.alignment
  vkBindBufferMemory(device, buffer, memory, 0);

  return memory;
}

struct HostToBufferCopyInfo {
  const void* data_src;
  VkDeviceSize data_size;
};

void CopyHostToBuffer(VkDeviceSize total_size,
                      const VkDeviceMemory& device_memory,
                      const VkDevice& device,
                      const std::vector<HostToBufferCopyInfo>& copy_infos) {
  // data transfer may not happen immediately, for example because it is only
  // written to cache and not yet to device. we can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // specify VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient)
  void* dst;
  vkMapMemory(device, device_memory, 0, total_size, 0, &dst);
  VkDeviceSize offset = 0;
  for (const auto& info : copy_infos) {
    memcpy(static_cast<char*>(dst) + offset, info.data_src,
           static_cast<size_t>(info.data_size));
    offset += info.data_size;
  }
  vkUnmapMemory(device, device_memory);
}

void CopyBufferToBuffer(VkDeviceSize data_size,
                        const VkBuffer& src_buffer,
                        const VkBuffer& dst_buffer,
                        const VkDevice& device,
                        const Queues::Queue& transfer_queue) {
  // construct command pool, buffer and record info
  VkCommandPool command_pool = CreateCommandPool(
      transfer_queue.family_index, device, true);

  // specify region to copy
  VkBufferCopy copy_region{
    .srcOffset = 0,
    .dstOffset = 0,
    .size = data_size,
  };

  // record command (just one-time copy)
  VkCommandBuffer command_buffer = CreateCommandBuffer(device, command_pool);
  VkCommandBufferBeginInfo begin_info{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(command_buffer, &begin_info);
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);
  vkEndCommandBuffer(command_buffer);

  // submit command buffers
  VkSubmitInfo submit_info{
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .commandBufferCount = 1,
    .pCommandBuffers = &command_buffer,
  };

  // execute, wait until finish and cleanup
  vkQueueSubmit(transfer_queue.queue, 1, &submit_info, VK_NULL_HANDLE);
  vkQueueWaitIdle(transfer_queue.queue);
  vkDestroyCommandPool(device, command_pool, nullptr);
}

} /* namespace */

void Buffer::Init(
    const void* vertex_data, size_t vertex_size, size_t vertex_count,
    const void*  index_data, size_t  index_size, size_t  index_count) {
  VkDeviceSize total_size = vertex_size + index_size;
  vertex_size_ = vertex_size;
  vertex_count_ = static_cast<uint32_t>(vertex_count);
  index_count_  = static_cast<uint32_t>(index_count);

  const VkDevice& device = *app_.device();
  const VkPhysicalDevice& physical_device = *app_.physical_device();

  // vertex/index buffer cannot be most efficient if it has to be visible to
  // both host and device, so we create vertex/index buffer that is only visible
  // to device, and staging buffer that is visible to both and transfers data
  // to vertex/index buffer
  VkBuffer staging_buffer = CreateBuffer(       // source of transfer
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total_size, device);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT       // host can access it
    | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,     // see host cache management
      staging_buffer, device, physical_device);

  // copy from host to staging buffer
  CopyHostToBuffer(total_size, staging_memory, device, {
    {.data_src = vertex_data, .data_size = vertex_size},
    {.data_src =  index_data, .data_size =  index_size},
  });

  // create final buffer that is only visible to device
  // for more efficient memory usage, we put vertex and index data in one buffer
  buffer_ = CreateBuffer(
      VK_BUFFER_USAGE_TRANSFER_DST_BIT          // destination of transfer
    | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
    | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      total_size, device);
  device_memory_ = CreateBufferMemory(
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,      // only accessible for device
      buffer_, device, physical_device);

  // copy from staging buffer to final buffer
  // graphics or compute queues implicitly have transfer capability
  CopyBufferToBuffer(total_size, staging_buffer, buffer_, device,
                     app_.queues().graphics);

  // cleanup transient objects
  vkDestroyBuffer(device, staging_buffer, nullptr);
  vkFreeMemory(device, staging_memory, nullptr);
}

void Buffer::Draw(const VkCommandBuffer& command_buffer) const {
  VkDeviceSize vertex_offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer_, &vertex_offset);
  vkCmdBindIndexBuffer(command_buffer, buffer_, vertex_size_,  // note offset
                       VK_INDEX_TYPE_UINT32);
  // (index_count, instance_count, first_index, vertex_offset, first_instance)
  vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
}

Buffer::~Buffer() {
  const VkDevice& device = *app_.device();
  vkDestroyBuffer(device, buffer_, nullptr);
  vkFreeMemory(device, device_memory_, nullptr);
}

} /* namespace wrapper */
} /* namespace vulkan */
