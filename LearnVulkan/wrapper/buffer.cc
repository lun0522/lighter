//
//  buffer.cc
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "buffer.h"

#include "context.h"
#include "command.h"
#include "util.h"

using std::vector;

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
    if (type_filter & (1 << i)) {  // type is suitable for buffer
      auto flags = properties.memoryTypes[i].propertyFlags;
      if ((flags & mem_properties) == mem_properties)  // has required property
        return i;
    }
  }
  throw std::runtime_error{"Failed to find suitable memory type"};
}

VkBuffer CreateBuffer(VkBufferUsageFlags buffer_usage,
                      VkDeviceSize data_size,
                      const VkDevice& device,
                      const VkAllocationCallbacks* allocator) {
  // create buffer
  VkBufferCreateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = data_size,
      .usage = buffer_usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,  // only graphics queue access
  };

  VkBuffer buffer{};
  ASSERT_SUCCESS(vkCreateBuffer(device, &buffer_info, allocator, &buffer),
                 "Failed to create buffer");
  return buffer;
}

VkDeviceMemory CreateBufferMemory(VkMemoryPropertyFlags mem_properties,
                                  const VkBuffer& buffer,
                                  const VkDevice& device,
                                  const VkPhysicalDevice& physical_device,
                                  const VkAllocationCallbacks* allocator) {
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
  ASSERT_SUCCESS(vkAllocateMemory(device, &memory_info, allocator, &memory),
                 "Failed to allocate buffer memory");

  // associate allocated memory with buffer
  // since this memory is specifically allocated for this buffer, the last
  // parameter |memoryOffset| is simply 0
  // otherwise it should be selected according to mem_requirements.alignment
  vkBindBufferMemory(device, buffer, memory, 0);

  return memory;
}

struct HostToBufferCopyInfo {
  const void* ptr;
  VkDeviceSize size;
  VkDeviceSize offset;
};

void CopyHostToBuffer(VkDeviceSize map_offset,
                      VkDeviceSize map_size,
                      const VkDeviceMemory& device_memory,
                      const VkDevice& device,
                      const vector<HostToBufferCopyInfo>& copy_infos) {
  // data transfer may not happen immediately, for example because it is only
  // written to cache and not yet to device. we can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // specify VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient)
  void* dst;
  vkMapMemory(device, device_memory, map_offset, map_size, 0, &dst);
  for (const auto& info : copy_infos)
    memcpy(static_cast<char*>(dst) + info.offset, info.ptr,
           static_cast<size_t>(info.size));
  vkUnmapMemory(device, device_memory);
}

void CopyBufferToBuffer(VkDeviceSize data_size,
                        const VkBuffer& src_buffer,
                        const VkBuffer& dst_buffer,
                        const VkDevice& device,
                        const Queues::Queue& transfer_queue,
                        const VkAllocationCallbacks* allocator) {
  // one-time copy command
  Command::OneTimeCommand(device, transfer_queue, allocator,
      [&](const VkCommandBuffer& command_buffer) {
        VkBufferCopy region{
            .srcOffset = 0,
            .dstOffset = 0,
            .size = data_size,
        };
        vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &region);
      }
  );
}

} /* namespace */

void VertexBuffer::Init(
    std::shared_ptr<Context> context,
    const void* vertex_data, size_t vertex_size, size_t vertex_count,
    const void*  index_data, size_t  index_size, size_t  index_count) {
  context_ = context;
  const VkDevice& device = *context_->device();
  const VkAllocationCallbacks* allocator = context_->allocator();

  VkDeviceSize total_size = vertex_size + index_size;
  vertex_size_  = vertex_size;
  vertex_count_ = static_cast<uint32_t>(vertex_count);
  index_count_  = static_cast<uint32_t>(index_count);

  // vertex/index buffer cannot be most efficient if it has to be visible to
  // both host and device, so we create vertex/index buffer that is only visible
  // to device, and staging buffer that is visible to both and transfers data
  // to vertex/index buffer
  VkBuffer staging_buffer = CreateBuffer(           // source of transfer
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, total_size, device, allocator);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT           // host can access it
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,   // see host cache management
      staging_buffer, device, *context_->physical_device(), allocator);

  // copy from host to staging buffer
  CopyHostToBuffer(0, total_size, staging_memory, device, {
      {.ptr = vertex_data, .size = vertex_size, .offset = 0},
      {.ptr =  index_data, .size =  index_size, .offset = vertex_size},
  });

  // create final buffer that is only visible to device
  // for more efficient memory usage, we put vertex and index data in one buffer
  buffer_ = CreateBuffer(
      VK_BUFFER_USAGE_TRANSFER_DST_BIT      // destination of transfer
          | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
          | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      total_size, device, allocator);
  device_memory_ = CreateBufferMemory(
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,  // only accessible for device
      buffer_, device, *context_->physical_device(), allocator);

  // copy from staging buffer to final buffer
  // graphics or compute queues implicitly have transfer capability
  CopyBufferToBuffer(total_size, staging_buffer, buffer_, device,
                     context_->queues().graphics, allocator);

  // cleanup transient objects
  vkDestroyBuffer(device, staging_buffer, allocator);
  vkFreeMemory(device, staging_memory, allocator);
}

void VertexBuffer::Draw(const VkCommandBuffer& command_buffer) const {
  static const VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer_, &offset);
  vkCmdBindIndexBuffer(command_buffer, buffer_, vertex_size_,  // note offset
                       VK_INDEX_TYPE_UINT32);
  // (index_count, instance_count, first_index, vertex_offset, first_instance)
  vkCmdDrawIndexed(command_buffer, index_count_, 1, 0, 0, 0);
}

VertexBuffer::~VertexBuffer() {
  vkDestroyBuffer(*context_->device(), buffer_, context_->allocator());
  vkFreeMemory(*context_->device(), device_memory_, context_->allocator());
}

void UniformBuffer::Init(
    std::shared_ptr<Context> context,
    const void* data, size_t num_chunk, size_t chunk_size) {
  context_ = context;
  const VkDevice& device = *context->device();
  const VkAllocationCallbacks* allocator = context_->allocator();

  data_ = static_cast<const char*>(data);
  // offset is required to be multiple of minUniformBufferOffsetAlignment
  // which is why we have actual data size |chunk_data_size_| and its
  // aligned size |chunk_memory_size_|
  VkDeviceSize alignment =
      context_->physical_device().limits().minUniformBufferOffsetAlignment;
  chunk_data_size_ = chunk_size;
  chunk_memory_size_ = (chunk_data_size_ + alignment) / alignment * alignment;

  buffer_ = CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         num_chunk * chunk_memory_size_, device, allocator);
  device_memory_ = CreateBufferMemory(
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      buffer_, device, *context_->physical_device(), allocator);

  VkDescriptorPoolSize pool_size{
      .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = static_cast<uint32_t>(num_chunk),
  };

  VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .poolSizeCount = 1,
      .pPoolSizes = &pool_size,
      .maxSets = static_cast<uint32_t>(num_chunk),
  };

  ASSERT_SUCCESS(
      vkCreateDescriptorPool(device, &pool_info, allocator, &descriptor_pool_),
      "Failed to create descriptor pool");

  VkDescriptorSetLayoutBinding layout_binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS,
  };

  VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 1,
      .pBindings = &layout_binding,
  };

  descriptor_set_layouts_.resize(num_chunk);
  for (auto& layout : descriptor_set_layouts_)
    ASSERT_SUCCESS(vkCreateDescriptorSetLayout(
                       device, &layout_info, allocator, &layout),
                   "Failed to create descriptor set layout");

  VkDescriptorSetAllocateInfo alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = descriptor_pool_,
      .descriptorSetCount = static_cast<uint32_t>(num_chunk),
      .pSetLayouts = descriptor_set_layouts_.data(),
  };

  descriptor_sets_.resize(num_chunk);
  ASSERT_SUCCESS(
      vkAllocateDescriptorSets(device, &alloc_info, descriptor_sets_.data()),
      "Failed to allocate descriptor set");

  for (size_t i = 0; i < num_chunk; ++i) {
    VkDescriptorBufferInfo buffer_info{
        .buffer = buffer_,
        .offset = chunk_memory_size_ * i,
        .range = chunk_data_size_,
    };
    VkWriteDescriptorSet write_descriptor_set{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptor_sets_[i],
      .dstBinding = 0,  // uniform buffer binding index
      .dstArrayElement = 0,  // target first descriptor in set
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .descriptorCount = 1,  // possible to update multiple descriptors
      .pBufferInfo = &buffer_info,
    };
    vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, nullptr);
  }
}

void UniformBuffer::Update(size_t chunk_index) const {
  VkDeviceSize src_offset = chunk_data_size_ * chunk_index;
  VkDeviceSize dst_offset = chunk_memory_size_ * chunk_index;
  CopyHostToBuffer(
      dst_offset, chunk_data_size_, device_memory_, *context_->device(),
      {{.ptr = data_ + src_offset, .size = chunk_data_size_, .offset = 0}});
}

void UniformBuffer::Bind(const VkCommandBuffer& command_buffer,
                         const VkPipelineLayout& pipeline_layout,
                         size_t chunk_index) const {
  vkCmdBindDescriptorSets(
      command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout,
      0, 1, &descriptor_sets_[chunk_index], 0, nullptr);
}

UniformBuffer::~UniformBuffer() {
  const VkDevice& device = *context_->device();
  const VkAllocationCallbacks* allocator = context_->allocator();
  vkDestroyDescriptorPool(device, descriptor_pool_, allocator);
  // descriptor sets are implicitly cleaned up with descriptor pool
  for (auto& layout : descriptor_set_layouts_)
    vkDestroyDescriptorSetLayout(device, layout, allocator);
  vkDestroyBuffer(device, buffer_, allocator);
  vkFreeMemory(device, device_memory_, allocator);
}

} /* namespace wrapper */
} /* namespace vulkan */
