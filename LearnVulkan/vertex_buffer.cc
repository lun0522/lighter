//
//  vertex_buffer.cc
//  LearnVulkan
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "vertex_buffer.h"

#include "application.h"

using namespace glm;
using namespace std;

namespace vulkan {

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
  throw runtime_error{"Failed to find suitable memory type"};
}

void CreateBuffer(VkBuffer* buffer,
                  VkDeviceMemory* device_memory,
                  VkDeviceSize data_size,
                  VkBufferUsageFlags buffer_usage,
                  VkSharingMode sharing_mode,
                  VkMemoryPropertyFlags mem_properties,
                  const VkDevice& device,
                  const VkPhysicalDevice& physical_device) {
  // create buffer
  VkBufferCreateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_info.size = data_size;
  buffer_info.usage = buffer_usage;
  buffer_info.sharingMode = sharing_mode;

  ASSERT_SUCCESS(vkCreateBuffer(device, &buffer_info, nullptr, buffer),
                 "Failed to create buffer");

  // query memory requirements for this buffer
  //   .size: size of required amount of memory
  //   .alignment: offset where this buffer begins in allocated region
  //   .memoryTypeBits: memory type suitable for this buffer
  VkMemoryRequirements mem_requirements{};
  vkGetBufferMemoryRequirements(device, *buffer, &mem_requirements);

  // allocate memory on device
  VkMemoryAllocateInfo mem_alloc_info{};
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.allocationSize = mem_requirements.size;
  mem_alloc_info.memoryTypeIndex = FindMemoryType(
    mem_requirements.memoryTypeBits, mem_properties, physical_device);

  ASSERT_SUCCESS(vkAllocateMemory(
                   device, &mem_alloc_info, nullptr, device_memory),
                 "Failed to allocate buffer memory");

  // associate allocated memory with buffer
  // since this memory is specifically allocated for this buffer, last
  // parameter (`memoryOffset`) is simply 0
  // otherwise it should be selected according to mem_requirements.alignment
  vkBindBufferMemory(device, *buffer, *device_memory, 0);
}

void CopyToDeviceMemory(const void* source,
                        VkDeviceSize size,
                        const VkDeviceMemory& memory,
                        const VkDevice& device) {
  // data transfer may not happen immediately, for example because it is only
  // written to cache and not yet to device. we can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // specify VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient)
  void* data;
  vkMapMemory(device, memory, 0, size, 0, &data);
  memcpy(data, source, static_cast<size_t>(size));
  vkUnmapMemory(device, memory);
}

} /* namespace */

const vector<VertexAttrib> kTriangleVertices {
  {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
  {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
  {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
};

array<VkVertexInputBindingDescription, 1> VertexAttrib::binding_descriptions() {
  array<VkVertexInputBindingDescription, 1> binding_descs{};

  binding_descs[0].binding = 0;
  binding_descs[0].stride = sizeof(VertexAttrib);
  // for instancing, use _INSTANCE for .inputRate
  binding_descs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  return binding_descs;
}

array<VkVertexInputAttributeDescription, 2> VertexAttrib::attrib_descriptions() {
  array<VkVertexInputAttributeDescription, 2> attrib_descs{};

  attrib_descs[0].binding = 0; // which binding point does data come from
  attrib_descs[0].location = 0; // layout (location = 0) in
  attrib_descs[0].format = VK_FORMAT_R32G32_SFLOAT; // implies total size
  attrib_descs[0].offset = offsetof(VertexAttrib, pos); // reading offset

  attrib_descs[1].binding = 0;
  attrib_descs[1].location = 1;
  attrib_descs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  attrib_descs[1].offset = offsetof(VertexAttrib, color);

  return attrib_descs;
}

void VertexBuffer::Init(const void* data,
                        size_t data_size,
                        size_t vertex_count) {
  vertex_count_ = static_cast<uint32_t>(vertex_count);
  CreateBuffer(&buffer_, &device_memory_, data_size,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
               VK_SHARING_MODE_EXCLUSIVE, // only graphics queue will access it
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT   // host can write to it
             | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, // host cache management
               *app_.device(), *app_.physical_device());
  CopyToDeviceMemory(data, data_size, device_memory_, *app_.device());
}

void VertexBuffer::Draw(const VkCommandBuffer& command_buffer) const {
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, 0, 1, &buffer_, &offset);
  // (vertex_count, instance_count, first_vertex, first_instance)
  vkCmdDraw(command_buffer, vertex_count_, 1, 0, 0);
}

VertexBuffer::~VertexBuffer() {
  const VkDevice& device = *app_.device();
  vkDestroyBuffer(device, buffer_, nullptr);
  vkFreeMemory(device, device_memory_, nullptr);
}

} /* namespace vulkan */
