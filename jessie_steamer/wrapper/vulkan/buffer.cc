//
//  buffer.cc
//
//  Created by Pujun Lun on 2/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/buffer.h"

#include <array>
#include <cstring>

#include "absl/strings/str_format.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/command.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::array;
using std::vector;

constexpr VkMemoryPropertyFlags kHostVisibleMemory =
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

// Returns an instance of util::QueueUsage that only involves a graphics queue.
inline util::QueueUsage GetGraphicsQueueUsage(
    const SharedBasicContext& context) {
  return util::QueueUsage{{context->queues().graphics_queue().family_index}};
}

// Returns an instance of util::QueueUsage that only involves a transfer queue.
inline util::QueueUsage GetTransferQueueUsage(
    const SharedBasicContext& context) {
  return util::QueueUsage{{context->queues().transfer_queue().family_index}};
}

// Returns the index of a VkMemoryType that satisfies both 'memory_type' and
// 'memory_properties' within VkPhysicalDeviceMemoryProperties.memoryTypes.
uint32_t FindMemoryTypeIndex(const SharedBasicContext& context,
                             uint32_t memory_type,
                             VkMemoryPropertyFlags memory_properties) {
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(*context->physical_device(), &properties);

  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if ((1U << i) & memory_type) {
      if ((properties.memoryTypes[i].propertyFlags & memory_properties) ==
          memory_properties) {
        return i;
      }
    }
  }
  FATAL("Failed to find suitable memory type");
}

// Creates a buffer of 'data_size' for 'buffer_usages'
VkBuffer CreateBuffer(const SharedBasicContext& context,
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
  ASSERT_SUCCESS(vkCreateBuffer(*context->device(), &buffer_info,
                                *context->allocator(), &buffer),
                 "Failed to create buffer");
  return buffer;
}

// Allocates device memory for 'buffer' with 'memory_properties'.
VkDeviceMemory CreateBufferMemory(const SharedBasicContext& context,
                                  const VkBuffer& buffer,
                                  VkMemoryPropertyFlags memory_properties) {
  const VkDevice& device = *context->device();

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

  const VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/memory_requirements.size,
      FindMemoryTypeIndex(context, memory_requirements.memoryTypeBits,
                          memory_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(vkAllocateMemory(device, &memory_info,
                                  *context->allocator(), &memory),
                 "Failed to allocate buffer memory");

  // Bind the allocated memory with 'buffer'. If this memory is used for
  // multiple buffers, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  vkBindBufferMemory(device, buffer, memory, /*memoryOffset=*/0);
  return memory;
}

// A collection of commonly used options when we create VkImage.
struct ImageConfig {
  explicit ImageConfig(bool need_access_to_texels = false) {
    if (need_access_to_texels) {
      // If we want to directly access texels of the image, we would use a
      // layout that preserves texels.
      tiling = VK_IMAGE_TILING_LINEAR;
      initial_layout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    } else {
      tiling = VK_IMAGE_TILING_OPTIMAL;
      initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    }
  }

  uint32_t mip_levels = kSingleMipLevel;
  uint32_t layer_count = kSingleImageLayer;
  VkSampleCountFlagBits sample_count = kSingleSample;
  VkImageTiling tiling;
  VkImageLayout initial_layout;
};

// Creates an image that can be used by the graphics queue.
VkImage CreateImage(const SharedBasicContext& context,
                    const ImageConfig& config,
                    VkImageCreateFlags flags,
                    VkFormat format,
                    VkExtent3D extent,
                    VkImageUsageFlags usage) {
  const util::QueueUsage queue_usage = GetGraphicsQueueUsage(context);
  const VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      flags,
      /*imageType=*/VK_IMAGE_TYPE_2D,
      format,
      extent,
      config.mip_levels,
      config.layer_count,
      config.sample_count,
      config.tiling,
      usage,
      queue_usage.sharing_mode(),
      queue_usage.unique_family_indices_count(),
      queue_usage.unique_family_indices(),
      config.initial_layout,
  };

  VkImage image;
  ASSERT_SUCCESS(vkCreateImage(*context->device(), &image_info,
                               *context->allocator(), &image),
                 "Failed to create image");
  return image;
}

// Allocates device memory for 'image' with 'memory_properties'.
VkDeviceMemory CreateImageMemory(const SharedBasicContext& context,
                                 const VkImage& image,
                                 VkMemoryPropertyFlags memory_properties) {
  const VkDevice& device = *context->device();

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(device, image, &memory_requirements);

  const VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/memory_requirements.size,
      FindMemoryTypeIndex(context, memory_requirements.memoryTypeBits,
                          memory_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(vkAllocateMemory(device, &memory_info,
                                  *context->allocator(), &memory),
                 "Failed to allocate image memory");

  // Bind the allocated memory with 'image'. If this memory is used for
  // multiple images, the memory offset should be re-calculated and
  // VkMemoryRequirements.alignment should be considered.
  vkBindImageMemory(device, image, memory, /*memoryOffset=*/0);
  return memory;
}

// Inserts a pipeline barrier for transitioning the image layout.
// This should be called when 'command_buffer' is recording commands.
void WaitForImageMemoryBarrier(
    const VkImageMemoryBarrier& barrier,
    const VkCommandBuffer& command_buffer,
    const array<VkPipelineStageFlags, 2>& pipeline_stages) {
  vkCmdPipelineBarrier(
      command_buffer,
      pipeline_stages[0],
      pipeline_stages[1],
      // Either 0 or VK_DEPENDENCY_BY_REGION_BIT. The latter one allows reading
      // from regions that have been written to, even if the entire writing has
      // not yet finished.
      /*dependencyFlags=*/0,
      /*memoryBarrierCount=*/0,
      /*pMemoryBarriers=*/nullptr,
      /*bufferMemoryBarrierCount=*/0,
      /*pBufferMemoryBarriers=*/nullptr,
      /*imageMemoryBarrierCount=*/1,
      &barrier);
}

// Transitions image layout using the transfer queue.
void TransitionImageLayout(
    const SharedBasicContext& context,
    const VkImage& image, const ImageConfig& image_config,
    VkImageAspectFlags image_aspect,
    const array<VkImageLayout, 2>& image_layouts,
    const array<VkAccessFlags, 2>& access_flags,
    const array<VkPipelineStageFlags, 2>& pipeline_stages) {
  const auto& transfer_queue = context->queues().transfer_queue();
  OneTimeCommand command{context, &transfer_queue};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    VkImageMemoryBarrier barrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /*pNext=*/nullptr,
        access_flags[0],
        access_flags[1],
        image_layouts[0],
        image_layouts[1],
        /*srcQueueFamilyIndex=*/transfer_queue.family_index,
        /*dstQueueFamilyIndex=*/transfer_queue.family_index,
        image,
        VkImageSubresourceRange{
            image_aspect,
            /*baseMipLevel=*/0,
            image_config.mip_levels,
            /*baseArrayLayer=*/0,
            image_config.layer_count,
        },
    };
    WaitForImageMemoryBarrier(barrier, command_buffer, pipeline_stages);
  });
}

// Maps device memory with the given 'map_offset' and 'map_size', and copies
// data from the host according to 'copy_infos'.
void CopyHostToBuffer(const SharedBasicContext& context,
                      VkDeviceSize map_offset,
                      VkDeviceSize map_size,
                      const VkDeviceMemory& device_memory,
                      const vector<Buffer::CopyInfo>& copy_infos) {
  // Data transfer may not happen immediately, for example, because it is only
  // written to cache and not yet to device. We can either flush host writes
  // with vkFlushMappedMemoryRanges and vkInvalidateMappedMemoryRanges, or
  // use VK_MEMORY_PROPERTY_HOST_COHERENT_BIT (a little less efficient).
  void* dst;
  vkMapMemory(*context->device(), device_memory, map_offset, map_size,
              /*flags=*/0, &dst);
  for (const auto& info : copy_infos) {
    std::memcpy(static_cast<char*>(dst) + info.offset, info.data, info.size);
  }
  vkUnmapMemory(*context->device(), device_memory);
}

// Copies data of 'data_size' from 'src_buffer' to 'dst_buffer' using the
// transfer queue.
void CopyBufferToBuffer(const SharedBasicContext& context,
                        VkDeviceSize data_size,
                        const VkBuffer& src_buffer,
                        const VkBuffer& dst_buffer) {
  OneTimeCommand command{context, &context->queues().transfer_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    VkBufferCopy region{
        /*srcOffset=*/0,
        /*dstOffset=*/0,
        data_size,
    };
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer,
                    /*regionCount=*/1, &region);
  });
}

// Copies data from the host to 'buffer', which is only visible to the device,
// using the transfer queue.
void CopyHostToBufferViaStaging(const SharedBasicContext& context,
                                const VkBuffer& buffer,
                                const Buffer::CopyInfos& copy_infos) {
  // Create staging buffer and associated memory, which is accessible from host.
  VkBuffer staging_buffer = CreateBuffer(
      context, copy_infos.total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      GetTransferQueueUsage(context));
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context, staging_buffer, kHostVisibleMemory);

  // Copy from host to staging buffer.
  CopyHostToBuffer(context, /*map_offset=*/0,
                   /*map_size=*/copy_infos.total_size,
                   staging_memory, copy_infos.copy_infos);

  // Copy from staging buffer to final buffer.
  CopyBufferToBuffer(context, copy_infos.total_size, staging_buffer, buffer);

  // Destroy transient objects.
  vkDestroyBuffer(*context->device(), staging_buffer, *context->allocator());
  vkFreeMemory(*context->device(), staging_memory, *context->allocator());
}

// Copies data from 'buffer' to 'image' using the transfer queue.
void CopyBufferToImage(const SharedBasicContext& context,
                       const VkBuffer& buffer,
                       const VkImage& image,
                       const ImageConfig& image_config,
                       const VkExtent3D& image_extent,
                       VkImageLayout image_layout) {
  OneTimeCommand command{context, &context->queues().transfer_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    VkBufferImageCopy region{
        // First three parameters specify pixels layout in buffer.
        // Setting all of them to 0 means pixels are tightly packed.
        /*bufferOffset=*/0,
        /*bufferRowLength=*/0,
        /*bufferImageHeight=*/0,
        VkImageSubresourceLayers{
            /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
            /*mipLevel=*/0,
            /*baseArrayLayer=*/0,
            image_config.layer_count,
        },
        VkOffset3D{/*x=*/0, /*y=*/0, /*z=*/0},
        image_extent,
    };
    vkCmdCopyBufferToImage(command_buffer, buffer, image, image_layout,
                           /*regionCount=*/1, &region);
  });
}

// Converts 2D dimension to 3D offset, where the expanded dimension is set to 1.
inline VkOffset3D ExtentToOffset(const VkExtent2D& extent) {
  return VkOffset3D{
      static_cast<int32_t>(extent.width),
      static_cast<int32_t>(extent.height),
      /*z=*/1,
  };
}

// Expands one dimension for 'extent', where the expanded dimension is set to 1.
inline VkExtent3D ExpandDimension(const VkExtent2D& extent) {
  return {extent.width, extent.height, /*depth=*/1};
}

// Returns extents of mipmaps. The original extent will not be included.
vector<VkExtent2D> GenerateMipmapExtents(const VkExtent3D& image_extent) {
  const int largest_dim = std::max(image_extent.width, image_extent.height);
  const int mip_levels = std::floor(static_cast<float>(std::log2(largest_dim)));
  vector<VkExtent2D> mipmap_extents(mip_levels);
  VkExtent2D extent{image_extent.width, image_extent.height};
  for (int level = 0; level < mip_levels; ++level) {
    extent.width = extent.width > 1 ? extent.width / 2 : 1;
    extent.height = extent.height > 1 ? extent.height / 2 : 1;
    mipmap_extents[level] = extent;
  }
  return mipmap_extents;
}

// Generates mipmaps for 'image' using the transfer queue.
void GenerateMipmaps(const SharedBasicContext& context,
                     const VkImage& image, VkFormat image_format,
                     const VkExtent3D& image_extent,
                     const vector<VkExtent2D>& mipmap_extents) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(*context->physical_device(),
                                      image_format, &properties);
  if (!(properties.optimalTilingFeatures &
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    FATAL("Image format does not support linear blitting");
  }

  const auto& transfer_queue = context->queues().transfer_queue();
  OneTimeCommand command{context, &transfer_queue};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    VkImageMemoryBarrier barrier{
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        /*pNext=*/nullptr,
        /*srcAccessMask=*/0,  // To be updated.
        /*dstAccessMask=*/0,  // To be updated.
        /*oldLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,  // To be updated.
        /*newLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,  // To be updated.
        /*srcQueueFamilyIndex=*/transfer_queue.family_index,
        /*dstQueueFamilyIndex=*/transfer_queue.family_index,
        image,
        VkImageSubresourceRange{
            /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
            /*baseMipLevel=*/0,  // To be updated.
            /*levelCount=*/1,
            /*baseArrayLayer=*/0,
            /*layerCount=*/1,
        },
    };

    uint32_t dst_level = 1;
    VkExtent2D prev_extent{image_extent.width, image_extent.height};
    for (const auto& extent : mipmap_extents) {
      const uint32_t src_level = dst_level - 1;

      // Transition the layout of previous layer to TRANSFER_SRC_OPTIMAL.
      barrier.subresourceRange.baseMipLevel = src_level;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      WaitForImageMemoryBarrier(barrier, command_buffer,
                                {VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT});

      // Blit the previous level to next level after transitioning is done.
      const VkImageBlit image_blit{
          /*srcSubresource=*/VkImageSubresourceLayers{
              /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
              /*mipLevel=*/src_level,
              /*baseArrayLayer=*/0,
              /*layerCount=*/1,
          },
          /*srcOffsets=*/{
              VkOffset3D{/*x=*/0, /*y=*/0, /*z=*/0},
              ExtentToOffset(prev_extent),
          },
          /*dstSubresource=*/VkImageSubresourceLayers{
              /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
              /*mipLevel=*/dst_level,
              /*baseArrayLayer=*/0,
              /*layerCount=*/1,
          },
          /*dstOffsets=*/{
              VkOffset3D{/*x=*/0, /*y=*/0, /*z=*/0},
              ExtentToOffset(extent),
          },
      };

      vkCmdBlitImage(command_buffer,
                     /*srcImage=*/image,
                     /*srcImageLayout=*/VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                     /*dstImage=*/image,
                     /*dstImageLayout=*/VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     /*regionCount=*/1, &image_blit,
                     /*filter=*/VK_FILTER_LINEAR);

      ++dst_level;
      prev_extent = extent;
    }

    // Transition the layout of all levels to SHADER_READ_ONLY_OPTIMAL.
    for (uint32_t level = 0; level < mipmap_extents.size() + 1; ++level) {
      barrier.subresourceRange.baseMipLevel = level;
      barrier.oldLayout = level == mipmap_extents.size()
                              ? VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
                              : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      WaitForImageMemoryBarrier(barrier, command_buffer,
                                {VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
  });
}

} /* namespace */

void VertexBuffer::CreateBufferAndMemory(VkDeviceSize total_size,
                                         bool is_dynamic) {
  VkBufferUsageFlags buffer_usages = VK_BUFFER_USAGE_INDEX_BUFFER_BIT
                                         | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
  VkMemoryPropertyFlags memory_properties;
  if (is_dynamic) {
    memory_properties = kHostVisibleMemory;
  } else {
    buffer_usages |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
  buffer_ = CreateBuffer(context_, total_size, buffer_usages,
                         GetGraphicsQueueUsage(context_));
  device_memory_ = CreateBufferMemory(context_, buffer_, memory_properties);
}

PerVertexBuffer::CopyInfos PerVertexBuffer::CreateCopyInfos(
    const BufferDataInfo& info) {
  mesh_data_infos_.clear();
  if (absl::holds_alternative<ShareIndicesDataInfo>(info)) {
    return CreateCopyInfos(absl::get<ShareIndicesDataInfo>(info));
  } else if (absl::holds_alternative<NoShareIndicesDataInfo>(info)) {
    return CreateCopyInfos(absl::get<NoShareIndicesDataInfo>(info));
  } else {
    FATAL("Unrecognized variant type");
  }
}

PerVertexBuffer::CopyInfos PerVertexBuffer::CreateCopyInfos(
    const ShareIndicesDataInfo& info) {
  // Vertex buffer layout (@ refers to the index of mesh):
  // | shared indices | vertices@0 | vertices@1 | vertices@2 | ...
  constexpr int kIndicesOffset = 0;
  const auto& vertices_info = info.per_mesh_vertices;
  const auto& indices_info = info.shared_indices;
  mesh_data_infos_.reserve(info.num_mesh);
  const VkDeviceSize initial_vertices_offset =
      kIndicesOffset + indices_info.size_per_mesh;
  VkDeviceSize vertices_offset = initial_vertices_offset;
  for (int i = 0; i < info.num_mesh; ++i) {
    mesh_data_infos_.emplace_back(MeshDataInfo{
        static_cast<uint32_t>(indices_info.num_unit_per_mesh),
        kIndicesOffset,
        vertices_offset,
    });
    vertices_offset += vertices_info.size_per_mesh;
  }
  return CopyInfos{
      /*total_size=*/vertices_offset,
      /*copy_infos=*/{
          CopyInfo{
              indices_info.data,
              indices_info.size_per_mesh,
              kIndicesOffset,
          },
          CopyInfo{
              vertices_info.data,
              vertices_info.size_per_mesh * info.num_mesh,
              initial_vertices_offset,
          },
      },
  };
}

PerVertexBuffer::CopyInfos PerVertexBuffer::CreateCopyInfos(
    const NoShareIndicesDataInfo& info) {
  // Vertex buffer layout (@ refers to the index of mesh):
  // | indices@0 | vertices@0 | indices@1 | vertices@1 | ...
  const auto& per_mesh_infos = info.per_mesh_infos;
  mesh_data_infos_.reserve(per_mesh_infos.size());
  vector<CopyInfo> copy_infos;
  copy_infos.reserve(per_mesh_infos.size() * 2);
  VkDeviceSize indices_offset = 0;
  for (const auto& mesh_info : per_mesh_infos) {
    const size_t indices_data_size = mesh_info.indices.size_per_mesh;
    const size_t vertices_data_size = mesh_info.vertices.size_per_mesh;
    const VkDeviceSize vertices_offset = indices_offset + indices_data_size;
    mesh_data_infos_.emplace_back(MeshDataInfo{
        static_cast<uint32_t>(mesh_info.indices.num_unit_per_mesh),
        indices_offset,
        vertices_offset,
    });
    copy_infos.emplace_back(CopyInfo{
        mesh_info.indices.data,
        indices_data_size,
        indices_offset,
    });
    copy_infos.emplace_back(CopyInfo{
        mesh_info.vertices.data,
        vertices_data_size,
        vertices_offset,
    });
    indices_offset += indices_data_size + vertices_data_size;
  }
  return CopyInfos{/*total_size=*/indices_offset, std::move(copy_infos)};
}

void PerVertexBuffer::Draw(const VkCommandBuffer& command_buffer,
                           int mesh_index, uint32_t instance_count) const {
  const MeshDataInfo& info = mesh_data_infos_[mesh_index];
  vkCmdBindIndexBuffer(command_buffer, buffer_, info.indices_offset,
                       VK_INDEX_TYPE_UINT32);
  vkCmdBindVertexBuffers(command_buffer, kPerVertexBindingPoint,
                         /*bindingCount=*/1, &buffer_, &info.vertices_offset);
  vkCmdDrawIndexed(command_buffer, info.indices_count, instance_count,
                   /*firstIndex=*/0, /*vertexOffset=*/0, /*firstInstance=*/0);
}

StaticPerVertexBuffer::StaticPerVertexBuffer(
    SharedBasicContext context, const BufferDataInfo& info)
    : PerVertexBuffer{std::move(context)} {
  const CopyInfos copy_infos = CreateCopyInfos(info);
  CreateBufferAndMemory(copy_infos.total_size, /*is_dynamic=*/false);
  CopyHostToBufferViaStaging(context_, buffer_, copy_infos);
}

void DynamicPerVertexBuffer::Reserve(int size) {
  if (size <= 0) {
    FATAL("Buffer size must be greater than 0");
  }
  if (size <= buffer_size_) {
    return;
  }

  if (buffer_size_ > 0) {
    // Make copy of 'buffer_' and 'device_memory_' since they will be changed.
    VkBuffer buffer = buffer_;
    VkDeviceMemory device_memory = device_memory_;
    context_->AddReleaseExpiredResourceOp(
        [buffer, device_memory](const BasicContext& context) {
          vkDestroyBuffer(*context.device(), buffer, *context.allocator());
          vkFreeMemory(*context.device(), device_memory, *context.allocator());
        });
  }
  buffer_size_ = size;
  CreateBufferAndMemory(buffer_size_, /*is_dynamic=*/true);
}

void DynamicPerVertexBuffer::Allocate(const BufferDataInfo& info) {
  const CopyInfos copy_infos = CreateCopyInfos(info);
  Reserve(copy_infos.total_size);
  CopyHostToBuffer(context_, /*map_offset=*/0, /*map_size=*/buffer_size_,
                   device_memory_, copy_infos.copy_infos);
}

PerInstanceBuffer::PerInstanceBuffer(SharedBasicContext context,
    const void* data, size_t data_size)
    : VertexBuffer{std::move(context)} {
  CreateBufferAndMemory(data_size, /*is_dynamic=*/false);
  CopyHostToBufferViaStaging(context_, buffer_, CopyInfos{
      /*total_size=*/data_size,
      /*copy_infos=*/{CopyInfo{data, data_size, /*offset=*/0}},
  });
}

void PerInstanceBuffer::Bind(const VkCommandBuffer& command_buffer,
                             uint32_t binding_point) const {
  constexpr VkDeviceSize kOffset = 0;
  vkCmdBindVertexBuffers(command_buffer, binding_point, /*bindingCount=*/1,
                         &buffer_, &kOffset);
}

UniformBuffer::UniformBuffer(SharedBasicContext context,
                             size_t chunk_size, int num_chunk)
    : DataBuffer{std::move(context)} {
  const VkDeviceSize alignment =
      context_->physical_device_limits().minUniformBufferOffsetAlignment;
  chunk_data_size_ = chunk_size;
  chunk_memory_size_ =
      (chunk_data_size_ + alignment - 1) / alignment * alignment;

  data_ = new char[chunk_data_size_ * num_chunk];
  buffer_ = CreateBuffer(context_, chunk_memory_size_ * num_chunk,
                         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         GetGraphicsQueueUsage(context_));
  device_memory_ = CreateBufferMemory(
      context_, buffer_, kHostVisibleMemory);
}

void UniformBuffer::Flush(int chunk_index) const {
  const VkDeviceSize src_offset = chunk_data_size_ * chunk_index;
  const VkDeviceSize dst_offset = chunk_memory_size_ * chunk_index;
  CopyHostToBuffer(
      context_, dst_offset, chunk_data_size_, device_memory_,
      {{data_ + src_offset, chunk_data_size_, /*offset=*/0}});
}

VkDescriptorBufferInfo UniformBuffer::GetDescriptorInfo(
    int chunk_index) const {
  return VkDescriptorBufferInfo{
      buffer_,
      /*offset=*/chunk_memory_size_ * chunk_index,
      /*range=*/chunk_data_size_,
  };
}

TextureBuffer::TextureBuffer(SharedBasicContext context,
                             bool generate_mipmaps, const Info& info)
    : ImageBuffer{std::move(context)}, mip_levels_{kSingleMipLevel} {
  const VkExtent3D image_extent = info.GetExtent3D();
  const VkDeviceSize data_size = info.GetDataSize();

  const auto layer_count = CONTAINER_SIZE(info.datas);
  if (layer_count != 1 && layer_count != kCubemapImageCount) {
    FATAL(absl::StrFormat("Invalid number of images: %d", layer_count));
  }

  // Generate mipmaps if requested.
  vector<VkExtent2D> mipmap_extents;
  if (generate_mipmaps) {
    mipmap_extents = GenerateMipmapExtents(image_extent);
    mip_levels_ = mipmap_extents.size() + 1;
  }

  // Create staging buffer and associated memory.
  VkBuffer staging_buffer = CreateBuffer(
      context_, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      GetTransferQueueUsage(context_));
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context_, staging_buffer, kHostVisibleMemory);

  // Copy from host to staging buffer.
  const VkDeviceSize image_size = data_size / layer_count;
  for (int i = 0; i < layer_count; ++i) {
    CopyHostToBuffer(context_, image_size * i, image_size, staging_memory, {
        {info.datas[i], image_size, /*offset=*/0},
    });
  }

  // Create final image buffer.
  const VkImageCreateFlags cubemap_flag = layer_count == kCubemapImageCount ?
      VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : nullflag;
  const VkImageUsageFlags mipmap_flag =
      generate_mipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : nullflag;

  ImageConfig image_config;
  image_config.mip_levels = mip_levels_;
  image_config.layer_count = layer_count;
  image_ = CreateImage(context_, image_config, cubemap_flag,
                       info.format, image_extent,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT
                           | mipmap_flag);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // Copy data from staging buffer to image buffer.
  TransitionImageLayout(
      context_, image_, image_config, VK_IMAGE_ASPECT_COLOR_BIT,
      {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
      {kNullAccessFlag, VK_ACCESS_TRANSFER_WRITE_BIT},
      {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT});
  CopyBufferToImage(context_, staging_buffer, image_, image_config,
                    image_extent, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  if (generate_mipmaps) {
    GenerateMipmaps(context_, image_, info.format,
                    image_extent, mipmap_extents);
  } else {
    TransitionImageLayout(
        context_, image_, image_config, VK_IMAGE_ASPECT_COLOR_BIT,
        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
        {VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT});
  }

  // Destroy transient objects.
  vkDestroyBuffer(*context_->device(), staging_buffer, *context_->allocator());
  vkFreeMemory(*context_->device(), staging_memory, *context_->allocator());
}

OffscreenBuffer::OffscreenBuffer(SharedBasicContext context,
                                 const VkExtent2D& extent, VkFormat format)
    : ImageBuffer{std::move(context)} {
  image_ = CreateImage(context_, ImageConfig{}, nullflag, format,
                       ExpandDimension(extent),
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

DepthStencilBuffer::DepthStencilBuffer(
    SharedBasicContext context, const VkExtent2D& extent, VkFormat format)
    : ImageBuffer{std::move(context)} {
  image_ = CreateImage(context_, ImageConfig{}, nullflag, format,
                       ExpandDimension(extent),
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

MultisampleBuffer::MultisampleBuffer(
    SharedBasicContext context,
    Type type, const VkExtent2D& extent, VkFormat format,
    VkSampleCountFlagBits sample_count)
    : ImageBuffer{std::move(context)} {
  VkImageUsageFlags image_usage;
  switch (type) {
    case Type::kColor:
      image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                        | VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
      break;
    case Type::kDepthStencil:
      image_usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      break;
  }
  ImageConfig image_config;
  image_config.sample_count = sample_count;
  image_ = CreateImage(context_, image_config, nullflag, format,
                       ExpandDimension(extent), image_usage);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

PushConstant::PushConstant(const SharedBasicContext& context,
                           size_t size_per_frame, int num_frame) {
  if (size_per_frame > kMaxPushConstantSize) {
    FATAL(absl::StrFormat(
        "Pushing constant of size %d bytes per-frame. To be compatible with "
        "all devices, the size should not be greater than %d bytes.",
        size_per_frame, kMaxPushConstantSize));
  }
  size_per_frame_ = static_cast<uint32_t>(size_per_frame);
  data_ = new char[size_per_frame_ * num_frame];
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
