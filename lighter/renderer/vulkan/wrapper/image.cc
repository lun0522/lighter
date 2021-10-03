//
//  image.cc
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/vulkan/wrapper/image.h"

#include <algorithm>

#include "lighter/renderer/vulkan/wrapper/command.h"
#include "lighter/renderer/vulkan/wrapper/image_util.h"
#include "third_party/absl/strings/str_format.h"

namespace lighter {
namespace renderer {
namespace vulkan {
namespace {

const int kSingleImageLayer = common::image::kSingleImageLayer;
const int kCubemapImageLayer = common::image::kCubemapImageLayer;

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

// Returns the first image format among 'candidates' that has the specified
// 'features'. If not found, returns std::nullopt.
std::optional<VkFormat> FindImageFormatWithFeature(
    const BasicContext& context,
    absl::Span<const VkFormat> candidates,
    VkFormatFeatureFlags features) {
  for (const auto format : candidates) {
    VkFormatProperties properties;
    vkGetPhysicalDeviceFormatProperties(*context.physical_device(),
                                        format, &properties);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return std::nullopt;
}

// Returns the image format to use for a color image given number of 'channel'.
// Only 1 or 4 channels are supported.
VkFormat FindColorImageFormat(
    const BasicContext& context,
    int channel, absl::Span<const ImageUsage> usages,
    bool use_high_precision = false) {
  switch (channel) {
    case common::image::kBwImageChannel: {
      // VK_FORMAT_R8_UNORM and VK_FORMAT_R16_SFLOAT have mandatory support for
      // sampling, but may not support linear access. We may switch to 4-channel
      // formats since they have mandatory support for both.
      VkFormat best_format, alternative_format;
      if (use_high_precision) {
        best_format = VK_FORMAT_R16_SFLOAT;
        alternative_format = VK_FORMAT_R16G16B16A16_SFLOAT;
      } else {
        best_format = VK_FORMAT_R8_UNORM;
        alternative_format = VK_FORMAT_R8G8B8A8_UNORM;
      }
      if (!ImageUsage::IsLinearAccessed(usages)) {
        return best_format;
      }
      if (FindImageFormatWithFeature(
              context, {best_format},
              VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT).has_value()) {
        return best_format;
      } else {
#ifndef NDEBUG
        LOG_INFO << "The single channel image format does not support linear "
                    "access, use the 4-channel format instead";
#endif /* !NDEBUG */
        return alternative_format;
      }
    }

    case common::image::kRgbaImageChannel:
      if (use_high_precision) {
        return VK_FORMAT_R16G16B16A16_SFLOAT;
      } else {
        return VK_FORMAT_R8G8B8A8_UNORM;
      }

    default:
      FATAL(absl::StrFormat(
          "Number of channels can only be 1 or 4, while %d provided", channel));
  }
}

// Returns the image format to use for a depth stencil image. If not found, a
// runtime exception will be thrown.
VkFormat FindDepthStencilImageFormat(const BasicContext& context) {
  const auto format = FindImageFormatWithFeature(
      context, {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
      VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
  ASSERT_HAS_VALUE(format, "Failed to find depth stencil image format");
  return format.value();
}

// Returns the maximum number of samples per pixel indicated by 'sample_counts'.
VkSampleCountFlagBits GetMaxSampleCount(VkSampleCountFlags sample_counts) {
  for (const auto count : {VK_SAMPLE_COUNT_64_BIT,
                           VK_SAMPLE_COUNT_32_BIT,
                           VK_SAMPLE_COUNT_16_BIT,
                           VK_SAMPLE_COUNT_8_BIT,
                           VK_SAMPLE_COUNT_4_BIT,
                           VK_SAMPLE_COUNT_2_BIT}) {
    if (sample_counts & count) {
      return count;
    }
  }
  FATAL("Multisampling is not supported by hardware");
}

// Creates a TextureBuffer::Info object, assuming all images have the same
// properties as the given 'sample_image'. The size of 'image_datas' can only be
// either 1 or 6 (for cubemaps)
TextureImage::Info CreateTextureBufferInfo(
    const BasicContext& context,
    const common::Image& image,
    absl::Span<const ImageUsage> usages) {
  return TextureImage::Info{
      image.GetDataPtrs(),
      FindColorImageFormat(context, image.channel(), usages),
      static_cast<uint32_t>(image.width()),
      static_cast<uint32_t>(image.height()),
      static_cast<uint32_t>(image.channel()),
      usages,
  };
}

// Creates an image that can be used by the graphics queue.
VkImage CreateImage(const BasicContext& context,
                    const ImageConfig& config,
                    VkImageCreateFlags flags,
                    VkFormat format,
                    const VkExtent3D& extent,
                    VkImageUsageFlags usage) {
  const auto queue_usage = context.queues().GetGraphicsQueueUsage();
  const VkImageCreateInfo image_info{
      VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      /*pNext=*/nullptr,
      flags,
      VK_IMAGE_TYPE_2D,
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
  ASSERT_SUCCESS(vkCreateImage(*context.device(), &image_info,
                               *context.allocator(), &image),
                 "Failed to create image");
  return image;
}

// Allocates device memory for 'image' with 'memory_properties'.
VkDeviceMemory CreateImageMemory(const BasicContext& context,
                                 const VkImage& image,
                                 VkMemoryPropertyFlags memory_properties) {
  const VkDevice& device = *context.device();

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(device, image, &memory_requirements);

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
    const std::array<VkPipelineStageFlags, 2>& pipeline_stages) {
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
    const std::array<VkImageLayout, 2>& image_layouts,
    const std::array<VkAccessFlags, 2>& access_flags,
    const std::array<VkPipelineStageFlags, 2>& pipeline_stages) {
  const auto& transfer_queue = context->queues().transfer_queue();
  const OneTimeCommand command{context, &transfer_queue};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    const VkImageMemoryBarrier barrier{
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
std::vector<VkExtent2D> GenerateMipmapExtents(const VkExtent3D& image_extent) {
  const int largest_dim = std::max(image_extent.width, image_extent.height);
  const int mip_levels = std::floor(static_cast<float>(std::log2(largest_dim)));
  std::vector<VkExtent2D> mipmap_extents(mip_levels);
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
                     const std::vector<VkExtent2D>& mipmap_extents) {
  VkFormatProperties properties;
  vkGetPhysicalDeviceFormatProperties(*context->physical_device(),
                                      image_format, &properties);
  ASSERT_TRUE(properties.optimalTilingFeatures &
                  VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT,
              "Image format does not support linear blitting");

  const auto& transfer_queue = context->queues().transfer_queue();
  const OneTimeCommand command{context, &transfer_queue};
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
            VK_IMAGE_ASPECT_COLOR_BIT,
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
              VK_IMAGE_ASPECT_COLOR_BIT,
              /*mipLevel=*/src_level,
              /*baseArrayLayer=*/0,
              /*layerCount=*/1,
          },
          /*srcOffsets=*/{
              VkOffset3D{/*x=*/0, /*y=*/0, /*z=*/0},
              ExtentToOffset(prev_extent),
          },
          /*dstSubresource=*/VkImageSubresourceLayers{
              VK_IMAGE_ASPECT_COLOR_BIT,
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
                     /*regionCount=*/1, &image_blit, VK_FILTER_LINEAR);

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

// Creates an image view to specify the usage of image data.
VkImageView CreateImageView(const BasicContext& context,
                            const VkImage& image,
                            VkFormat format,
                            VkImageAspectFlags image_aspect,
                            uint32_t mip_levels,
                            uint32_t layer_count) {
  VkImageViewType view_type;
  switch (layer_count) {
    case kSingleImageLayer:
      view_type = VK_IMAGE_VIEW_TYPE_2D;
      break;
    case kCubemapImageLayer:
      view_type = VK_IMAGE_VIEW_TYPE_CUBE;
      break;
    default:
      FATAL(absl::StrFormat("Unsupported layer count: %d", layer_count));
  }

  const VkImageViewCreateInfo image_view_info{
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      image,
      view_type,
      format,
      // Swizzle color channels around.
      VkComponentMapping{
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY,
          VK_COMPONENT_SWIZZLE_IDENTITY
      },
      // Specify image's purpose and which part to access.
      VkImageSubresourceRange{
          image_aspect,
          /*baseMipLevel=*/0,
          mip_levels,
          /*baseArrayLayer=*/0,
          layer_count,
      },
  };

  VkImageView image_view;
  ASSERT_SUCCESS(vkCreateImageView(*context.device(), &image_view_info,
                                   *context.allocator(), &image_view),
                 "Failed to create image view");
  return image_view;
}

// Creates an image sampler.
VkSampler CreateSampler(const BasicContext& context,
                        int mip_levels, const ImageSampler::Config& config) {
  // 'mipLodBias', 'minLod' and 'maxLod' are used to control mipmapping.
  const VkSamplerCreateInfo sampler_info{
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      /*pNext=*/nullptr,
      /*flags=*/nullflag,
      /*magFilter=*/config.filter,
      /*minFilter=*/config.filter,
      VK_SAMPLER_MIPMAP_MODE_LINEAR,
      /*addressModeU=*/config.address_mode,
      /*addressModeV=*/config.address_mode,
      /*addressModeW=*/config.address_mode,
      /*mipLodBias=*/0.0f,
      /*anisotropyEnable=*/VK_TRUE,
      // Max amount of texel samples used for anisotropy.
      /*maxAnisotropy=*/16,
      // May compare texels with a certain value and use result for filtering.
      /*compareEnable=*/VK_FALSE,
      /*compareOp=*/VK_COMPARE_OP_ALWAYS,
      /*minLod=*/0.0f,
      /*maxLod=*/static_cast<float>(mip_levels),
      VK_BORDER_COLOR_INT_OPAQUE_BLACK,
      /*unnormalizedCoordinates=*/VK_FALSE,
  };

  VkSampler sampler;
  ASSERT_SUCCESS(vkCreateSampler(*context.device(), &sampler_info,
                                 *context.allocator(), &sampler),
                 "Failed to create sampler");
  return sampler;
}

} /* namespace */

void ImageStagingBuffer::CopyToImage(const VkImage& target,
                                     const VkExtent3D& image_extent,
                                     uint32_t image_layer_count) const {
  const OneTimeCommand command{context_, &context_->queues().transfer_queue()};
  command.Run([&](const VkCommandBuffer& command_buffer) {
    const VkBufferImageCopy region{
        // First three parameters specify pixels layout in buffer.
        // Setting all of them to 0 means pixels are tightly packed.
        /*bufferOffset=*/0,
        /*bufferRowLength=*/0,
        /*bufferImageHeight=*/0,
        VkImageSubresourceLayers{
            VK_IMAGE_ASPECT_COLOR_BIT,
            /*mipLevel=*/0,
            /*baseArrayLayer=*/0,
            image_layer_count,
        },
        VkOffset3D{/*x=*/0, /*y=*/0, /*z=*/0},
        image_extent,
    };
    vkCmdCopyBufferToImage(command_buffer, buffer(), target,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           /*regionCount=*/1, &region);
  });
}

ImageSampler::ImageSampler(SharedBasicContext context,
                           int mip_levels, const Config& config)
    : context_{std::move(FATAL_IF_NULL(context))},
      sampler_{CreateSampler(*context_, mip_levels, config)} {}

Buffer::CopyInfos TextureImage::Info::GetCopyInfos() const {
  const VkDeviceSize single_image_data_size = width * height * channel;
  const VkDeviceSize total_data_size =
      single_image_data_size * data_ptrs.size();
  std::vector<Buffer::CopyInfo> copy_infos(data_ptrs.size());
  for (int i = 0; i < copy_infos.size(); ++i) {
    copy_infos[i] = {data_ptrs[i], single_image_data_size,
                     /*offset=*/single_image_data_size * i};
  }
  return {total_data_size, std::move(copy_infos)};
}

TextureImage::TextureImage(SharedBasicContext context,
                           bool generate_mipmaps,
                           const ImageSampler::Config& sampler_config,
                           const Info& info)
    : Image{std::move(FATAL_IF_NULL(context)), info.GetExtent2D(), info.format},
      buffer_{context_, generate_mipmaps, info},
      sampler_{context_, buffer_.mip_levels(), sampler_config} {
  set_image_view(CreateImageView(
      *context_, buffer_.image(), format_, VK_IMAGE_ASPECT_COLOR_BIT,
      buffer_.mip_levels(), /*layer_count=*/CONTAINER_SIZE(info.data_ptrs)));
}

TextureImage::TextureImage(const SharedBasicContext& context,
                           bool generate_mipmaps,
                           const common::Image& image,
                           absl::Span<const ImageUsage> usages,
                           const ImageSampler::Config& sampler_config)
    : TextureImage{context, generate_mipmaps, sampler_config,
                   CreateTextureBufferInfo(*FATAL_IF_NULL(context), image,
                                           usages)} {}

TextureImage::TextureBuffer::TextureBuffer(
    SharedBasicContext context, bool generate_mipmaps, const Info& info)
    : ImageBuffer{std::move(FATAL_IF_NULL(context))} {
  const VkExtent3D image_extent = info.GetExtent3D();
  const auto layer_count = CONTAINER_SIZE(info.data_ptrs);
  ASSERT_TRUE(layer_count == kSingleImageLayer ||
                  layer_count == kCubemapImageLayer,
              absl::StrFormat("Invalid number of images: %d", layer_count));

  ImageConfig image_config;
  image_config.layer_count = CONTAINER_SIZE(info.data_ptrs);

  // Generate mipmap extents if requested.
  std::vector<VkExtent2D> mipmap_extents;
  if (generate_mipmaps) {
    mipmap_extents = GenerateMipmapExtents(image_extent);
    mip_levels_ = image_config.mip_levels = mipmap_extents.size() + 1;
  }

  // Create image buffer.
  VkImageCreateFlags create_flags = nullflag;
  if (layer_count == kCubemapImageLayer) {
    create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  auto usage_flags = image::GetImageUsageFlags(info.usages);
  usage_flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (generate_mipmaps) {
    usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }

  set_image(CreateImage(*context_, image_config, create_flags, info.format,
                        image_extent, usage_flags));
  set_device_memory(CreateImageMemory(
      *context_, image(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

  // Copy data from host to image buffer via staging buffer.
  TransitionImageLayout(
      context_, image(), image_config, VK_IMAGE_ASPECT_COLOR_BIT,
      {VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL},
      {kNullAccessFlag, VK_ACCESS_TRANSFER_WRITE_BIT},
      {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT});

  const ImageStagingBuffer staging_buffer{context_, info.GetCopyInfos()};
  staging_buffer.CopyToImage(image(), image_extent, image_config.layer_count);

  if (generate_mipmaps) {
    GenerateMipmaps(context_, image(), info.format,
                    image_extent, mipmap_extents);
  } else {
    TransitionImageLayout(
        context_, image(), image_config, VK_IMAGE_ASPECT_COLOR_BIT,
        {VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT},
        {VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT});
  }
}

SharedTexture::RefCountedTexture SharedTexture::GetTexture(
    const SharedBasicContext& context,
    const SourcePath& source_path,
    absl::Span<const ImageUsage> usages,
    const ImageSampler::Config& sampler_config) {
  FATAL_IF_NULL(context);
  context->RegisterAutoReleasePool<SharedTexture::RefCountedTexture>("texture");

  bool generate_mipmaps;
  const std::string* identifier;
  std::unique_ptr<common::Image> image;

  if (const auto* single_tex_path = std::get_if<SingleTexPath>(&source_path);
      single_tex_path != nullptr) {
    generate_mipmaps = true;
    identifier = single_tex_path;
    image = std::make_unique<common::Image>(
        common::Image::LoadSingleImageFromFile(*single_tex_path,
                                               /*flip_y=*/false));
  } else if (const auto* cubemap_path = std::get_if<CubemapPath>(&source_path);
             cubemap_path != nullptr) {
    generate_mipmaps = false;
    identifier = &cubemap_path->directory;
    image = std::make_unique<common::Image>(
        common::Image::LoadCubemapFromFiles(
            cubemap_path->directory, cubemap_path->files, /*flip_y=*/false));
  } else {
    FATAL("Unrecognized variant type");
  }

  return RefCountedTexture::Get(
      *identifier, context, generate_mipmaps, sampler_config,
      CreateTextureBufferInfo(*context, *image, usages));
}

OffscreenImage::OffscreenImage(SharedBasicContext context,
                               const VkExtent2D& extent, VkFormat format,
                               absl::Span<const ImageUsage> usages,
                               const ImageSampler::Config& sampler_config)
    : Image{std::move(FATAL_IF_NULL(context)), extent, format},
      buffer_{context_, extent_, format_, usages},
      sampler_{context_, kSingleMipLevel, sampler_config} {
  set_image_view(CreateImageView(*context_, buffer_.image(), format_,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 kSingleMipLevel, kSingleImageLayer));
}

OffscreenImage::OffscreenImage(const SharedBasicContext& context,
                               const VkExtent2D& extent, int channel,
                               absl::Span<const ImageUsage> usages,
                               const ImageSampler::Config& sampler_config,
                               bool use_high_precision)
    : OffscreenImage{context, extent,
                     FindColorImageFormat(*FATAL_IF_NULL(context),
                                          channel, usages, use_high_precision),
                     usages, sampler_config} {}

OffscreenImage::OffscreenBuffer::OffscreenBuffer(
    SharedBasicContext context,
    const VkExtent2D& extent, VkFormat format,
    absl::Span<const ImageUsage> usages)
    : ImageBuffer{std::move(context)} {
  set_image(CreateImage(*context_, ImageConfig{}, nullflag, format,
                        ExpandDimension(extent),
                        image::GetImageUsageFlags(usages)));
  set_device_memory(CreateImageMemory(
      *context_, image(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
}

DepthStencilImage::DepthStencilImage(const SharedBasicContext& context,
                                     const VkExtent2D& extent)
    : Image{context, extent, FindDepthStencilImageFormat(*context)},
      buffer_{context_, extent_, format_} {
  set_image_view(CreateImageView(*context_, buffer_.image(), format_,
                                 VK_IMAGE_ASPECT_DEPTH_BIT
                                     | VK_IMAGE_ASPECT_STENCIL_BIT,
                                 kSingleMipLevel, kSingleImageLayer));
}

DepthStencilImage::DepthStencilBuffer::DepthStencilBuffer(
    SharedBasicContext context, const VkExtent2D& extent, VkFormat format)
    : ImageBuffer{std::move(context)} {
  set_image(CreateImage(*context_, ImageConfig{}, nullflag, format,
                        ExpandDimension(extent),
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT));
  set_device_memory(CreateImageMemory(
      *context_, image(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
}

SwapchainImage::SwapchainImage(SharedBasicContext context,
                               const VkImage& image,
                               const VkExtent2D& extent, VkFormat format)
    : Image{std::move(context), extent, format}, image_{image} {
  set_image_view(CreateImageView(*context_, image_, format_,
                                 VK_IMAGE_ASPECT_COLOR_BIT,
                                 kSingleMipLevel, kSingleImageLayer));
}

std::unique_ptr<Image> MultisampleImage::CreateColorMultisampleImage(
    SharedBasicContext context,
    const Image& target_image, Mode mode) {
  return std::unique_ptr<Image>(new MultisampleImage{
      std::move(context), target_image.extent(), target_image.format(),
      mode, MultisampleBuffer::Type::kColor});
}

std::unique_ptr<Image> MultisampleImage::CreateDepthStencilMultisampleImage(
    SharedBasicContext context,
    const VkExtent2D& extent, Mode mode) {
  const VkFormat format = FindDepthStencilImageFormat(*context);
  return std::unique_ptr<Image>(new MultisampleImage{
      std::move(context), extent, format, mode,
      MultisampleBuffer::Type::kDepthStencil});
}

std::unique_ptr<Image> MultisampleImage::CreateDepthStencilImage(
    SharedBasicContext context,
    const VkExtent2D& extent, std::optional<Mode> mode) {
  if (mode.has_value()) {
    return CreateDepthStencilMultisampleImage(std::move(context),
                                              extent, mode.value());
  } else {
    return std::make_unique<DepthStencilImage>(std::move(context), extent);
  }
}

MultisampleImage::MultisampleImage(
    SharedBasicContext context,
    const VkExtent2D& extent, VkFormat format,
    Mode mode, MultisampleBuffer::Type type)
    : Image{std::move(context), extent, format},
      sample_count_{ChooseSampleCount(mode)},
      buffer_{context_, type, extent_, format_, sample_count_} {
  VkImageAspectFlags image_aspect;
  switch (type) {
    case MultisampleBuffer::Type::kColor:
      image_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
      break;
    case MultisampleBuffer::Type::kDepthStencil:
      image_aspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      break;
  }
  set_image_view(CreateImageView(
      *context_, buffer_.image(), format_,
      image_aspect, kSingleMipLevel, kSingleImageLayer));
}

VkSampleCountFlagBits MultisampleImage::ChooseSampleCount(Mode mode) {
  const auto& limits = context_->physical_device_limits();
  const VkSampleCountFlags sample_count_flag = std::min({
      limits.framebufferColorSampleCounts,
      limits.framebufferDepthSampleCounts,
      limits.framebufferStencilSampleCounts,
  });
  const VkSampleCountFlagBits max_sample_count =
      GetMaxSampleCount(sample_count_flag);
  switch (mode) {
    case Mode::kEfficient:
      return std::min(VK_SAMPLE_COUNT_4_BIT, max_sample_count);
    case Mode::kBestEffect:
      return max_sample_count;
  }
}

MultisampleImage::MultisampleBuffer::MultisampleBuffer(
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
  set_image(CreateImage(*context_, image_config, nullflag, format,
                        ExpandDimension(extent), image_usage));
  set_device_memory(CreateImageMemory(
      *context_, image(), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
}

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */
