//
//  buffer.cc
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/buffer.h"

#include <cstring>

#include "lighter/renderer/vulkan/wrapper/command.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

// Creates a buffer of 'data_size' for 'buffer_usages'
VkBuffer CreateBuffer(const BasicContext& context,
                      VkDeviceSize data_size,
                      VkBufferUsageFlags buffer_usages,
                      const util::QueueUsage& queue_usage) {
  const VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      data_size,
      buffer_usages,
      queue_usage.sharing_mode(),
      queue_usage.unique_family_indices_count(),
      queue_usage.unique_family_indices(),
  };

  VkBuffer buffer;
  ASSERT_SUCCESS(vkCreateBuffer(*context.device(), &buffer_info,
                                *context.allocator(), &buffer),
                 "Failed to create buffer");
  return buffer;
}

// Allocates device memory for 'buffer' with 'memory_properties'.
VkDeviceMemory CreateBufferMemory(const BasicContext& context,
                                  const VkBuffer& buffer,
                                  VkMemoryPropertyFlags memory_properties) {
  const VkDevice& device = *context.device();

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

  const VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/memory_requirements.size,
      util::FindMemoryTypeIndex(*context.physical_device(),
                                memory_requirements.memoryTypeBits,
                                memory_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(vkAllocateMemory(device, &memory_info,
                                  *context.allocator(), &memory),
                 "Failed to allocate buffer memory");

  // Bind the allocated memory with 'buffer'. If this memory is used for
  // multiple buffers, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  vkBindBufferMemory(device, buffer, memory, /*memoryOffset=*/0);
  return memory;
}

// Maps device memory with the given 'map_offset' and 'map_size', and copies
// data from the host according to 'copy_infos'.
void CopyHostToBuffer(const BasicContext& context,
                      VkDeviceSize map_offset,
                      VkDeviceSize map_size,
                      const VkDeviceMemory& device_memory,
                      const std::vector<Buffer::CopyInfo>& copy_infos) {
  // Data transfer may not happen immediately, for example, because it is only
  // written to cache and not yet to device. We can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient).
  void* dst;
  vkMapMemory(*context.device(), device_memory, map_offset, map_size,
              /*flags=*/0, &dst);
  for (const auto& info : copy_infos) {
    std::memcpy(static_cast<char*>(dst) + info.offset, info.data, info.size);
  }
  vkUnmapMemory(*context.device(), device_memory);
}

} /* namespace */

StagingBuffer::StagingBuffer(SharedBasicContext context,
                             const CopyInfos& copy_infos)
    : DataBuffer{std::move(FATAL_IF_NULL(context))},
      data_size_{copy_infos.total_size} {
  set_buffer(CreateBuffer(*context_, data_size_,
                          VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                          context_->queues().GetTransferQueueUsage()));
  set_device_memory(CreateBufferMemory(
      *context_, buffer(), kHostVisibleMemory));
  CopyHostToBuffer(*context_, /*map_offset=*/0, /*map_size=*/data_size_,
                   device_memory(), copy_infos.copy_infos);
}

void StagingBuffer::CopyToBuffer(const VkBuffer& target) const {
  const OneTimeCommand command{context_, &context_->queues().transfer_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    const VkBufferCopy region{/*srcOffset=*/0, /*dstOffset=*/0, data_size_};
    vkCmdCopyBuffer(command_buffer, buffer(), target,
                    /*regionCount=*/1, &region);
  });
}

std::vector<VkVertexInputAttributeDescription> VertexBuffer::GetAttributes(
    uint32_t start_location) const {
  std::vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(attributes_.size());
  for (const auto& attribute : attributes_) {
    descriptions.push_back(VkVertexInputAttributeDescription{
        start_location++,
        /*binding=*/0,  // To be updated.
        attribute.format,
        attribute.offset,
    });
  }
  return descriptions;
}

void VertexBuffer::DrawWithoutBuffer(
    const VkCommandBuffer& command_buffer,
    uint32_t vertex_count, uint32_t instance_count) {
  vkCmdDraw(command_buffer, vertex_count, instance_count,
            /*firstVertex=*/0, /*firstInstance=*/0);
}

void VertexBuffer::CreateBufferAndMemory(VkDeviceSize total_size,
                                         bool is_dynamic, bool has_index_data) {
  VkBufferUsageFlags buffer_usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  VkMemoryPropertyFlags memory_properties;
  if (is_dynamic) {
    memory_properties = kHostVisibleMemory;
  } else {
    buffer_usages |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
  if (has_index_data) {
    buffer_usages |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  }
  set_buffer(CreateBuffer(*context_, total_size, buffer_usages,
                          context_->queues().GetGraphicsQueueUsage()));
  set_device_memory(CreateBufferMemory(*context_, buffer(), memory_properties));
}

DynamicBuffer::DynamicBuffer(size_t initial_size, bool has_index_data,
                             VertexBuffer* vertex_buffer)
    : has_index_data_{has_index_data},
      vertex_buffer_{FATAL_IF_NULL(vertex_buffer)} {
  Reserve(initial_size);
}

void DynamicBuffer::Reserve(size_t size) {
  if (size <= buffer_size_) {
    return;
  }

  if (buffer_size_ > 0) {
    // Make copy of 'buffer_' and 'device_memory_' since they will be changed.
    auto buffer = vertex_buffer_->buffer();
    auto device_memory = vertex_buffer_->device_memory();
    vertex_buffer_->AddReleaseExpiredResourceOp(
        [buffer, device_memory](const BasicContext& context) {
          vkDestroyBuffer(*context.device(), buffer, *context.allocator());
          vkFreeMemory(*context.device(), device_memory, *context.allocator());
        });
  }
  buffer_size_ = size;
  vertex_buffer_->CreateBufferAndMemory(buffer_size_, /*is_dynamic=*/true,
                                        has_index_data_);
}

Buffer::CopyInfos PerVertexBuffer::NoIndicesDataInfo::CreateCopyInfos(
    PerVertexBuffer* buffer) const {
  // Vertex buffer layout (@ refers to the index of mesh):
  // | vertices@0 | vertices@1 | vertices@2 | ...
  auto& mesh_infos = buffer->mutable_mesh_data_infos()
                           ->emplace<MeshDataInfosNoIndices>().infos;
  mesh_infos.reserve(per_mesh_vertices_.size());
  std::vector<Buffer::CopyInfo> copy_infos;
  copy_infos.reserve(per_mesh_vertices_.size());

  VkDeviceSize offset = 0;
  for (const auto& vertices : per_mesh_vertices_) {
    mesh_infos.push_back(MeshDataInfosNoIndices::Info{
        static_cast<uint32_t>(vertices.num_units_per_mesh),
        offset,
    });
    copy_infos.push_back(Buffer::CopyInfo{
        vertices.data,
        vertices.size_per_mesh,
        offset,
    });
    offset += vertices.size_per_mesh;
  }

  return Buffer::CopyInfos{/*total_size=*/offset, std::move(copy_infos)};
}

Buffer::CopyInfos PerVertexBuffer::ShareIndicesDataInfo::CreateCopyInfos(
    PerVertexBuffer* buffer) const {
  // Vertex buffer layout (@ refers to the index of mesh):
  // | shared indices | vertices@0 | vertices@1 | vertices@2 | ...
  auto& mesh_infos = buffer->mutable_mesh_data_infos()
                           ->emplace<MeshDataInfosWithIndices>().infos;
  mesh_infos.reserve(num_meshes_);

  constexpr int kIndicesOffset = 0;
  const VkDeviceSize initial_vertices_offset =
      kIndicesOffset + shared_indices_.size_per_mesh;
  VkDeviceSize vertices_offset = initial_vertices_offset;
  for (int i = 0; i < num_meshes_; ++i) {
    mesh_infos.push_back(MeshDataInfosWithIndices::Info{
        static_cast<uint32_t>(shared_indices_.num_units_per_mesh),
        kIndicesOffset,
        vertices_offset,
    });
    vertices_offset += per_mesh_vertices_.size_per_mesh;
  }

  return Buffer::CopyInfos{
      /*total_size=*/vertices_offset,
      /*copy_infos=*/{
          Buffer::CopyInfo{
              shared_indices_.data,
              shared_indices_.size_per_mesh,
              kIndicesOffset,
          },
          Buffer::CopyInfo{
              per_mesh_vertices_.data,
              per_mesh_vertices_.size_per_mesh * num_meshes_,
              initial_vertices_offset,
          },
      },
  };
}

Buffer::CopyInfos PerVertexBuffer::NoShareIndicesDataInfo::CreateCopyInfos(
    PerVertexBuffer* buffer) const {
  // Vertex buffer layout (@ refers to the index of mesh):
  // | indices@0 | vertices@0 | indices@1 | vertices@1 | ...
  auto& mesh_infos = buffer->mutable_mesh_data_infos()
                           ->emplace<MeshDataInfosWithIndices>().infos;
  mesh_infos.reserve(per_mesh_infos_.size());
  std::vector<Buffer::CopyInfo> copy_infos;
  copy_infos.reserve(per_mesh_infos_.size() * 2);

  VkDeviceSize indices_offset = 0;
  for (const auto& mesh_info : per_mesh_infos_) {
    const size_t indices_data_size = mesh_info.indices.size_per_mesh;
    const size_t vertices_data_size = mesh_info.vertices.size_per_mesh;
    const VkDeviceSize vertices_offset = indices_offset + indices_data_size;
    mesh_infos.push_back(MeshDataInfosWithIndices::Info{
        static_cast<uint32_t>(mesh_info.indices.num_units_per_mesh),
        indices_offset,
        vertices_offset,
    });
    copy_infos.push_back(Buffer::CopyInfo{
        mesh_info.indices.data,
        indices_data_size,
        indices_offset,
    });
    copy_infos.push_back(Buffer::CopyInfo{
        mesh_info.vertices.data,
        vertices_data_size,
        vertices_offset,
    });
    indices_offset += indices_data_size + vertices_data_size;
  }

  return Buffer::CopyInfos{
      /*total_size=*/indices_offset,
      std::move(copy_infos),
  };
}

void PerVertexBuffer::Draw(const VkCommandBuffer& command_buffer,
                           uint32_t binding_point,
                           int mesh_index, uint32_t instance_count) const {
  if (const auto* mesh_on_indices =
          std::get_if<MeshDataInfosNoIndices>(&mesh_data_infos_);
      mesh_on_indices != nullptr) {
    const auto& mesh_info = mesh_on_indices->infos[mesh_index];
    vkCmdBindVertexBuffers(command_buffer, binding_point, /*bindingCount=*/1,
                           &buffer(), &mesh_info.vertices_offset);
    vkCmdDraw(command_buffer, mesh_info.vertices_count, instance_count,
              /*firstVertex=*/0, /*firstInstance=*/0);
  } else if (const auto* mesh_with_indices =
                 std::get_if<MeshDataInfosWithIndices>(&mesh_data_infos_);
             mesh_with_indices != nullptr) {
    const auto& mesh_info = mesh_with_indices->infos[mesh_index];
    vkCmdBindIndexBuffer(command_buffer, buffer(), mesh_info.indices_offset,
                         VK_INDEX_TYPE_UINT32);
    vkCmdBindVertexBuffers(command_buffer, binding_point, /*bindingCount=*/1,
                           &buffer(), &mesh_info.vertices_offset);
    vkCmdDrawIndexed(command_buffer, mesh_info.indices_count, instance_count,
                     /*firstIndex=*/0, /*vertexOffset=*/0, /*firstInstance=*/0);
  }
}

StaticPerVertexBuffer::StaticPerVertexBuffer(
    SharedBasicContext context, const BufferDataInfo& info,
    std::vector<Attribute>&& attributes)
    : PerVertexBuffer{std::move(context), std::move(attributes)} {
  const CopyInfos copy_infos = info.CreateCopyInfos(this);
  CreateBufferAndMemory(copy_infos.total_size, /*is_dynamic=*/false,
                        info.has_index_data());
  const StagingBuffer staging_buffer(context_, copy_infos);
  staging_buffer.CopyToBuffer(buffer());
}

void DynamicPerVertexBuffer::CopyHostData(const BufferDataInfo& info) {
  const CopyInfos copy_infos = info.CreateCopyInfos(this);
  Reserve(copy_infos.total_size);
  CopyHostToBuffer(*context_, /*map_offset=*/0, /*map_size=*/buffer_size(),
                   device_memory(), copy_infos.copy_infos);
}

void PerInstanceBuffer::Bind(const VkCommandBuffer& command_buffer,
                             uint32_t binding_point, int offset) const {
  const VkDeviceSize size_offset = per_instance_data_size_ * offset;
  vkCmdBindVertexBuffers(command_buffer, binding_point, /*bindingCount=*/1,
                         &buffer(), &size_offset);
}

StaticPerInstanceBuffer::StaticPerInstanceBuffer(
    SharedBasicContext context, uint32_t per_instance_data_size,
    const void* data, uint32_t num_instances,
    std::vector<Attribute>&& attributes)
    : PerInstanceBuffer{std::move(context), per_instance_data_size,
                        std::move(attributes)} {
  const uint32_t total_size = per_instance_data_size * num_instances;
  CreateBufferAndMemory(total_size, /*is_dynamic=*/false,
                        /*has_index_data=*/false);

  const CopyInfos copy_infos{
      total_size, /*copy_infos=*/{CopyInfo{data, total_size, /*offset=*/0}}};
  const StagingBuffer staging_buffer(context_, copy_infos);
  staging_buffer.CopyToBuffer(buffer());
}

void DynamicPerInstanceBuffer::CopyHostData(
    const void* data, uint32_t num_instances) {
  const uint32_t total_size = per_instance_data_size() * num_instances;
  const CopyInfos copy_infos{
      total_size, /*copy_infos=*/{CopyInfo{data, total_size, /*offset=*/0}},
  };
  Reserve(total_size);
  CopyHostToBuffer(*context_, /*map_offset=*/0, /*map_size=*/buffer_size(),
                   device_memory(), copy_infos.copy_infos);
}

UniformBuffer::UniformBuffer(SharedBasicContext context,
                             size_t chunk_size, int num_chunks)
    : DataBuffer{std::move(context)},
      chunk_data_size_{chunk_size}, num_chunks_{num_chunks} {
  const VkDeviceSize alignment =
      context_->physical_device_limits().minUniformBufferOffsetAlignment;
  chunk_memory_size_ =
      (chunk_data_size_ + alignment - 1) / alignment * alignment;

  data_ = new char[chunk_data_size_ * num_chunks];
  set_buffer(CreateBuffer(*context_, chunk_memory_size_ * num_chunks,
                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                          context_->queues().GetGraphicsQueueUsage()));
  set_device_memory(CreateBufferMemory(
      *context_, buffer(), kHostVisibleMemory));
}

void UniformBuffer::Flush(int chunk_index) const {
  ValidateChunkIndex(chunk_index);
  const VkDeviceSize src_offset = chunk_data_size_ * chunk_index;
  const VkDeviceSize dst_offset = chunk_memory_size_ * chunk_index;
  CopyHostToBuffer(
      *context_, dst_offset, chunk_data_size_, device_memory(),
      {{data_ + src_offset, chunk_data_size_, /*offset=*/0}});
}

void UniformBuffer::Flush(int chunk_index, VkDeviceSize data_size,
                          VkDeviceSize offset) const {
  ValidateChunkIndex(chunk_index);
  const VkDeviceSize src_offset = chunk_data_size_ * chunk_index + offset;
  const VkDeviceSize dst_offset = chunk_memory_size_ * chunk_index + offset;
  CopyHostToBuffer(
      *context_, dst_offset, data_size, device_memory(),
      {{data_ + src_offset, data_size, /*offset=*/0}});
}

VkDescriptorBufferInfo UniformBuffer::GetDescriptorInfo(
    int chunk_index) const {
  ValidateChunkIndex(chunk_index);
  return VkDescriptorBufferInfo{
      buffer(),
      /*offset=*/chunk_memory_size_ * chunk_index,
      /*range=*/chunk_data_size_,
  };
}

void UniformBuffer::ValidateChunkIndex(int chunk_index) const {
  ASSERT_TRUE(chunk_index < num_chunks_,
              absl::StrFormat("Chunk index (%d) of out range (%d)",
                              chunk_index, num_chunks_));
}

PushConstant::PushConstant(const SharedBasicContext& context,
                           size_t size_per_frame, int num_frames_in_flight)
    : size_per_frame_{static_cast<uint32_t>(size_per_frame)},
      num_frames_{num_frames_in_flight} {
  ASSERT_TRUE(size_per_frame <= kMaxPushConstantSize,
              absl::StrFormat("Pushing constant of size %d bytes per-frame. To "
                              "be compatible with all devices, the size should "
                              "not be greater than %d bytes.",
                              size_per_frame, kMaxPushConstantSize));
  data_ = new char[size_per_frame_ * num_frames_in_flight];
}

void PushConstant::Flush(const VkCommandBuffer& command_buffer,
                         const VkPipelineLayout& pipeline_layout,
                         int frame, uint32_t target_offset,
                         VkShaderStageFlags shader_stage) const {
  ValidateFrame(frame);
  vkCmdPushConstants(command_buffer, pipeline_layout, shader_stage,
                     target_offset, size_per_frame_, HostData<void>(frame));
}

void PushConstant::ValidateFrame(int frame) const {
  ASSERT_TRUE(frame < num_frames_,
              absl::StrFormat("Frame (%d) of out range (%d)",
                              frame, num_frames_));
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
