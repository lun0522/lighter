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

uint32_t FindMemoryType(const SharedBasicContext& context,
                        uint32_t type_filter,
                        VkMemoryPropertyFlags memory_properties) {
  // query available types of memory
  //   .memoryHeaps: memory heaps from which memory can be allocated
  //   .memoryTypes: memory types that can be used to access memory allocated
  //                 from heaps
  VkPhysicalDeviceMemoryProperties properties;
  vkGetPhysicalDeviceMemoryProperties(*context->physical_device(), &properties);

  for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
    if (type_filter & (1U << i)) {  // types is suitable for buffer
      auto flags = properties.memoryTypes[i].propertyFlags;
      if ((flags & memory_properties) ==
           memory_properties) {  // has required property
        return i;
      }
    }
  }
  FATAL("Failed to find suitable memory type");
}

VkBuffer CreateBuffer(const SharedBasicContext& context,
                      VkDeviceSize data_size,
                      VkBufferUsageFlags buffer_usages) {
  // create buffer
  VkBufferCreateInfo buffer_info{
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      data_size,
      buffer_usages,
      /*sharingMode=*/VK_SHARING_MODE_EXCLUSIVE,  // only graphics queue access
      /*queueFamilyIndexCount=*/0,
      /*pQueueFamilyIndices=*/nullptr,
  };

  VkBuffer buffer;
  ASSERT_SUCCESS(vkCreateBuffer(*context->device(), &buffer_info,
                                *context->allocator(), &buffer),
                 "Failed to create buffer");
  return buffer;
}

VkDeviceMemory CreateBufferMemory(const SharedBasicContext& context,
                                  const VkBuffer& buffer,
                                  VkMemoryPropertyFlags memory_properties) {
  const VkDevice& device = *context->device();

  // query memory requirements for this buffer
  //   .size: size of required amount of memory
  //   .alignment: offset where this buffer begins in allocated region
  //   .memoryTypeBits: memory types suitable for this buffer
  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, buffer, &memory_requirements);

  // allocate memory on device
  VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/memory_requirements.size,
      /*memoryTypeIndex=*/FindMemoryType(
          context, memory_requirements.memoryTypeBits, memory_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(
      vkAllocateMemory(device, &memory_info, *context->allocator(), &memory),
      "Failed to allocate buffer memory");

  // associate allocated memory with buffer
  // since this memory is specifically allocated for this buffer, the last
  // parameter 'memoryOffset' is simply 0
  // otherwise it should be selected according to mem_requirements.alignment
  vkBindBufferMemory(device, buffer, memory, 0);

  return memory;
}

VkImage CreateImage(const SharedBasicContext& context,
                    VkImageCreateFlags flags,
                    VkFormat format,
                    VkExtent3D extent,
                    VkImageUsageFlags usage,
                    uint32_t mip_levels,
                    uint32_t layer_count,
                    VkSampleCountFlagBits sample_count) {
  VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      flags,
      /*imageType=*/VK_IMAGE_TYPE_2D,
      format,
      extent,
      mip_levels,
      layer_count,
      sample_count,
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
                               *context->allocator(), &image),
                 "Failed to create image");
  return image;
}

VkDeviceMemory CreateImageMemory(const SharedBasicContext& context,
                                 const VkImage& image,
                                 VkMemoryPropertyFlags memory_properties) {
  const VkDevice& device = *context->device();

  // query memory requirements for this image
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(device, image, &memory_requirements);

  // allocate memory on device
  VkMemoryAllocateInfo memory_info{
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      /*pNext=*/nullptr,
      /*allocationSize=*/memory_requirements.size,
      /*memoryTypeIndex=*/FindMemoryType(
          context, memory_requirements.memoryTypeBits, memory_properties),
  };

  VkDeviceMemory memory;
  ASSERT_SUCCESS(
      vkAllocateMemory(device, &memory_info, *context->allocator(), &memory),
      "Failed to allocate image memory");
  vkBindImageMemory(device, image, memory, 0);
  return memory;
}

void WaitForImageMemoryBarrier(
    const VkImageMemoryBarrier& barrier,
    const VkCommandBuffer& command_buffer,
    const array<VkPipelineStageFlags, 2>& pipeline_stages) {
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

void TransitionImageLayout(
    const SharedBasicContext& context,
    const VkImage& image, VkImageAspectFlags image_aspect,
    const array<VkImageLayout, 2>& image_layouts,
    const array<VkAccessFlags, 2>& barrier_access_flags,
    const array<VkPipelineStageFlags, 2>& pipeline_stages,
    uint32_t mip_levels, uint32_t layer_count) {
  const auto& transfer_queue = context->queues().transfer_queue();

  OneTimeCommand command{context, &transfer_queue};
  command.Run([&](const VkCommandBuffer& command_buffer) {
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
            image_aspect,
            /*baseMipLevel=*/0,
            mip_levels,
            /*baseArrayLayer=*/0,
            layer_count,
        },
    };
    WaitForImageMemoryBarrier(barrier, command_buffer, pipeline_stages);
  });
}

void CopyHostToBuffer(const SharedBasicContext& context,
                      VkDeviceSize map_offset,
                      VkDeviceSize map_size,
                      const VkDeviceMemory& device_memory,
                      const vector<Buffer::CopyInfo>& copy_infos) {
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
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &region);
  });
}

void CopyHostToBufferViaStaging(const SharedBasicContext& context,
                                const VkBuffer& buffer,
                                const Buffer::CopyInfos& infos) {
  // create staging buffer and associated memory
  VkBuffer staging_buffer = CreateBuffer(           // source of transfer
      context, infos.total_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  VkDeviceMemory staging_memory = CreateBufferMemory(
      context, staging_buffer,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT           // host can access it
          | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);  // see host cache management

  // copy from host to staging buffer
  CopyHostToBuffer(context, /*map_offset=*/0, /*map_size=*/infos.total_size,
                   staging_memory, infos.copy_infos);

  // copy from staging buffer to final buffer
  // graphics or compute queues implicitly have transfer capability
  CopyBufferToBuffer(context, infos.total_size, staging_buffer, buffer);

  // cleanup transient objects
  vkDestroyBuffer(*context->device(), staging_buffer, *context->allocator());
  vkFreeMemory(*context->device(), staging_memory, *context->allocator());
}

void CopyBufferToImage(const SharedBasicContext& context,
                       const VkBuffer& buffer,
                       const VkImage& image,
                       const VkExtent3D& image_extent,
                       VkImageLayout image_layout,
                       uint32_t layer_count) {
  OneTimeCommand command{context, &context->queues().transfer_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
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
  });
}

inline VkOffset3D ExtentToOffset(const VkExtent2D& extent) {
  return VkOffset3D{
      static_cast<int32_t>(extent.width),
      static_cast<int32_t>(extent.height),
      /*z=*/1,
  };
}

vector<VkExtent2D> GenerateMipmapExtents(const VkExtent3D& image_extent) {
  int largest_dim = std::max(image_extent.width, image_extent.height);
  int mip_levels = std::floor(static_cast<float>(std::log2(largest_dim)));
  vector<VkExtent2D> mipmap_extents(mip_levels);
  VkExtent2D extent{image_extent.width, image_extent.height};
  for (int level = 0; level < mip_levels; ++level) {
    extent.width = extent.width > 1 ? extent.width / 2 : 1;
    extent.height = extent.height > 1 ? extent.height / 2 : 1;
    mipmap_extents[level] = extent;
  }
  return mipmap_extents;
}

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
        /*srcAccessMask=*/0,  // to be updated
        /*dstAccessMask=*/0,  // to be updated
        /*oldLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,  // to be updated
        /*newLayout=*/VK_IMAGE_LAYOUT_UNDEFINED,  // to be updated
        /*srcQueueFamilyIndex=*/transfer_queue.family_index,
        /*dstQueueFamilyIndex=*/transfer_queue.family_index,
        image,
        // specify which part of image to use
        VkImageSubresourceRange{
            /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
            /*baseMipLevel=*/0,  // to be updated
            /*levelCount=*/1,
            /*baseArrayLayer=*/0,
            /*layerCount=*/1,
        },
    };

    uint32_t dst_level = 1;
    VkExtent2D prev_extent{image_extent.width, image_extent.height};
    for (const auto& extent : mipmap_extents) {
      const uint32_t src_level = dst_level - 1;

      // make the previous layer TRANSFER_SRC_OPTIMAL
      barrier.subresourceRange.baseMipLevel = src_level;
      barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      WaitForImageMemoryBarrier(barrier, command_buffer,
                                {VK_PIPELINE_STAGE_TRANSFER_BIT,
                                 VK_PIPELINE_STAGE_TRANSFER_BIT});

      // blit the previous level to next level after transitioning done
      VkImageBlit image_blit{
          /*srcSubresource=*/VkImageSubresourceLayers{
              /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
              /*mipLevel=*/src_level,
              /*baseArrayLayer=*/0,
              /*layerCount=*/1,
          },
          /*srcOffsets=*/{
              VkOffset3D{0, 0, 0},
              ExtentToOffset(prev_extent),
          },
          /*dstSubresource=*/VkImageSubresourceLayers{
              /*aspectMask=*/VK_IMAGE_ASPECT_COLOR_BIT,
              /*mipLevel=*/dst_level,
              /*baseArrayLayer=*/0,
              /*layerCount=*/1,
          },
          /*dstOffsets=*/{
              VkOffset3D{0, 0, 0},
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

    // make all levels SHADER_READ_ONLY_OPTIMAL
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
  VkBufferUsageFlags buffer_usages = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                         | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
  VkMemoryPropertyFlags memory_properties;
  if (is_dynamic) {
    memory_properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                            | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  } else {
    buffer_usages |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  }
  buffer_ = CreateBuffer(context_, total_size, buffer_usages);
  device_memory_ = CreateBufferMemory(context_, buffer_, memory_properties);
}

PerVertexBuffer::CopyInfos PerVertexBuffer::CreateCopyInfos(const Info& info) {
  if (absl::holds_alternative<InfoNoReuse>(info)) {
    return CreateCopyInfos(absl::get<InfoNoReuse>(info));
  } else if (absl::holds_alternative<InfoReuse>(info)) {
    return CreateCopyInfos(absl::get<InfoReuse>(info));
  } else {
    FATAL("Unrecognized variant type");
  }
}

PerVertexBuffer::CopyInfos PerVertexBuffer::CreateCopyInfos(
    const InfoNoReuse& info_no_reuse) {
  // vertex buffer layout (@ refers to the index of mesh):
  // | vertices@0 | indices@0 | vertices@1 | indices@1 | ...
  const auto& per_mesh_infos = info_no_reuse.per_mesh_infos;
  mesh_datas_.reserve(per_mesh_infos.size());
  vector<CopyInfo> copy_infos;
  copy_infos.reserve(per_mesh_infos.size() * 2);
  VkDeviceSize vertices_offset = 0;
  for (const auto& info : per_mesh_infos) {
    const auto vertices_data_size = info.vertices.GetTotalSize();
    const auto indices_data_size = info.indices.GetTotalSize();
    mesh_datas_.emplace_back(MeshData{
        vertices_offset,
        /*indices_offset=*/vertices_offset + vertices_data_size,
        static_cast<uint32_t>(info.indices.unit_count),
    });
    copy_infos.emplace_back(CopyInfo{
        info.vertices.data,
        vertices_data_size,
        /*offset=*/vertices_offset,
    });
    copy_infos.emplace_back(CopyInfo{
        info.indices.data,
        indices_data_size,
        /*offset=*/vertices_offset + vertices_data_size,
    });
    vertices_offset += vertices_data_size + indices_data_size;
  }
  return CopyInfos{/*total_size=*/vertices_offset, std::move(copy_infos)};
}

PerVertexBuffer::CopyInfos PerVertexBuffer::CreateCopyInfos(
    const InfoReuse& info_reuse) {
  // vertex buffer layout (@ refers to the index of mesh):
  // | shared indices | vertices@0 | vertices@1 | vertices@2 | ...
  constexpr int kIndicesOffset = 0;
  const auto& vertices_infos = info_reuse.per_mesh_vertices;
  const auto& indices_info = info_reuse.shared_indices;
  const auto vertices_data_size = vertices_infos.GetTotalSize();
  const auto indices_data_size = indices_info.GetTotalSize();
  mesh_datas_.reserve(info_reuse.num_mesh);
  VkDeviceSize vertices_offset = kIndicesOffset + indices_data_size;
  for (int i = 0; i < info_reuse.num_mesh; ++i) {
    mesh_datas_.emplace_back(MeshData{
        vertices_offset,
        kIndicesOffset,
        static_cast<uint32_t>(indices_info.unit_count),
    });
    vertices_offset += vertices_data_size;
  }
  return CopyInfos{
      /*total_size=*/vertices_offset,
      /*copy_infos=*/{
          CopyInfo{
              indices_info.data,
              indices_data_size,
              kIndicesOffset,
          },
          CopyInfo{
              vertices_infos.data,
              vertices_data_size * info_reuse.num_mesh,
              kIndicesOffset + indices_data_size,
          },
      },
  };
}

void PerVertexBuffer::Draw(const VkCommandBuffer& command_buffer,
                           int mesh_index, uint32_t instance_count) const {
  const MeshData& data = mesh_datas_[mesh_index];
  vkCmdBindVertexBuffers(command_buffer, kPerVertexBindingPoint,
                         /*bindingCount=*/1, &buffer_, &data.vertices_offset);
  vkCmdBindIndexBuffer(command_buffer, buffer_, data.indices_offset,
                       VK_INDEX_TYPE_UINT32);
  vkCmdDrawIndexed(command_buffer, data.indices_count, instance_count,
                   /*firstIndex=*/0, /*vertexOffset=*/0, /*firstInstance=*/0);
}

StaticPerVertexBuffer::StaticPerVertexBuffer(
    SharedBasicContext context, const Info& info)
    : PerVertexBuffer{std::move(context)} {
  CopyInfos infos = CreateCopyInfos(info);
  CreateBufferAndMemory(infos.total_size, /*is_dynamic=*/false);
  CopyHostToBufferViaStaging(context_, buffer_, infos);
}

DynamicPerVertexBuffer::DynamicPerVertexBuffer(
    SharedBasicContext context, int initial_size)
    : PerVertexBuffer(std::move(context)) {
  if (initial_size <= 0) {
    FATAL(absl::StrFormat(
        "Initial size must be greater than 0 (provided %d)", initial_size));
  }
  Reserve(initial_size);
}

void DynamicPerVertexBuffer::Reserve(int size) {
  if (size <= buffer_size_) {
    return;
  }

  if (buffer_size_ > 0) {
    // make copy of them since they will be changed soon.
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

void DynamicPerVertexBuffer::Allocate(const Info& info) {
  mesh_datas_.clear();
  CopyInfos infos = CreateCopyInfos(info);
  Reserve(infos.total_size);
  CopyHostToBuffer(context_, /*map_offset=*/0, /*map_size=*/buffer_size_,
                   device_memory_, infos.copy_infos);
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
  static constexpr VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(command_buffer, binding_point, /*bindingCount=*/1,
                         &buffer_, &offset);
}

UniformBuffer::UniformBuffer(SharedBasicContext context,
                             size_t chunk_size, int num_chunk)
    : DataBuffer{std::move(context)} {
  // offset is required to be multiple of minUniformBufferOffsetAlignment
  // which is why we have actual data size 'chunk_data_size_' and its
  // aligned size 'chunk_memory_size_'
  VkDeviceSize alignment =
      context_->physical_device_limits().minUniformBufferOffsetAlignment;
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
      context_, dst_offset, chunk_data_size_, device_memory_,
      {{data_ + src_offset, chunk_data_size_, /*offset=*/0}});
}

VkDescriptorBufferInfo UniformBuffer::descriptor_info(
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
  VkExtent3D image_extent = info.extent_3d();
  VkDeviceSize data_size = info.data_size();

  auto layer_count = CONTAINER_SIZE(info.datas);
  if (layer_count != 1 && layer_count != kCubemapImageCount) {
    FATAL(absl::StrFormat("Invalid number of images: %d", layer_count));
  }

  // generate mipmaps if required
  vector<VkExtent2D> mipmap_extents;
  if (generate_mipmaps) {
    mipmap_extents = GenerateMipmapExtents(image_extent);
    mip_levels_ = mipmap_extents.size() + 1;
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
    CopyHostToBuffer(context_, image_size * i, image_size, staging_memory, {
        {info.datas[i], image_size, /*offset=*/0},
    });
  }

  // create final image buffer
  const VkImageCreateFlags cubemap_flag = layer_count == kCubemapImageCount ?
      VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : nullflag;
  const VkImageUsageFlags mipmap_flag =
      generate_mipmaps ? VK_IMAGE_USAGE_TRANSFER_SRC_BIT : nullflag;
  image_ = CreateImage(context_, cubemap_flag, info.format, image_extent,
                       VK_IMAGE_USAGE_TRANSFER_DST_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT
                           | mipmap_flag,
                       mip_levels_, layer_count, kSingleSample);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  // copy data from staging buffer to image buffer
  TransitionImageLayout(
      context_, image_, VK_IMAGE_ASPECT_COLOR_BIT,
      {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
      {kNullAccessFlag, VK_ACCESS_TRANSFER_WRITE_BIT},
      {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT},
      mip_levels_, layer_count);
  CopyBufferToImage(context_, staging_buffer, image_, image_extent,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layer_count);
  if (generate_mipmaps) {
    GenerateMipmaps(context_, image_, info.format,
                    image_extent, mipmap_extents);
  } else {
    TransitionImageLayout(
        context_, image_, VK_IMAGE_ASPECT_COLOR_BIT,
        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
        {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT},
        kSingleMipLevel, layer_count);
  }

  // cleanup transient objects
  vkDestroyBuffer(*context_->device(), staging_buffer, *context_->allocator());
  vkFreeMemory(*context_->device(), staging_memory, *context_->allocator());
}

OffscreenBuffer::OffscreenBuffer(SharedBasicContext context,
                                 VkFormat format, VkExtent2D extent)
    : ImageBuffer{std::move(context)} {
  image_ = CreateImage(context_, nullflag, format,
                       {extent.width, extent.height, /*depth=*/1},
                       VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                           | VK_IMAGE_USAGE_SAMPLED_BIT,
                       kSingleMipLevel, kSingleImageLayer, kSingleSample);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

DepthStencilBuffer::DepthStencilBuffer(SharedBasicContext context,
                                       VkExtent2D extent, VkFormat format)
    : ImageBuffer{std::move(context)} {
  image_ = CreateImage(context_, nullflag, format,
                       {extent.width, extent.height, /*depth=*/1},
                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                       kSingleMipLevel, kSingleImageLayer, kSingleSample);
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
  image_ = CreateImage(
      context_, nullflag, format, {extent.width, extent.height, /*depth=*/1},
      image_usage, kSingleMipLevel, kSingleImageLayer, sample_count);
  device_memory_ = CreateImageMemory(
      context_, image_, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}

PushConstant::PushConstant(const SharedBasicContext& context,
                           size_t chunk_size, int num_chunk) {
  const int max_push_constant_size =
      context->physical_device_limits().maxPushConstantsSize;
  if (chunk_size > max_push_constant_size) {
    FATAL(absl::StrFormat(
        "Pushing constant of size %d bytes. To be compatible with all devices, "
        "the size should NOT be greater than %d bytes.",
        chunk_size, max_push_constant_size));
  }
  size_ = static_cast<uint32_t>(chunk_size);
  data_ = new char[size_ * num_chunk];
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
