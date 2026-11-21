#include "lighter/renderer/vk/image.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <optional>
#include <unordered_set>
#include <vector>

#include "lighter/renderer/vk/buffer_util.h"
#include "lighter/renderer/vk/image_util.h"
#include "lighter/renderer/vk/type_mapping.h"

namespace lighter::renderer::vk {
namespace {

using ir::ImageUsage;
using ir::MultisamplingMode;

const int kSingleMipLevel = base::image::kSingleMipLevel;

// Returns the first image format among 'candidates' that has the specified
// 'features'. If not found, returns std::nullopt.
std::optional<intl::Format> FindImageFormatWithFeature(
    const Context& context, std::span<const intl::Format> candidates,
    intl::FormatFeatureFlags features) {
  for (const intl::Format format : candidates) {
    const intl::FormatProperties properties =
        context.physical_device()->getFormatProperties(format);
    if ((properties.optimalTilingFeatures & features) == features) {
      return format;
    }
  }
  return std::nullopt;
}

intl::Format ChooseColorImageFormat(const Context& context, int channel,
                                    bool high_precision,
                                    std::span<const ImageUsage> usages) {
  switch (channel) {
    case base::image::kBwImageChannel: {
      const intl::Format best_format =
          high_precision ? intl::Format::eR16Sfloat : intl::Format::eR8Unorm;
      if (!ImageUsage::IsLinearAccessed(usages)) {
        return best_format;
      }

      // VK_FORMAT_R8_UNORM and VK_FORMAT_R16_SFLOAT have mandatory support for
      // sampling, but may not support linear access. We switch to 4-channel
      // formats instead since they have mandatory support for both.
      if (FindImageFormatWithFeature(context, std::array{best_format},
                                     intl::FormatFeatureFlagBits::eStorageImage)
              .has_value()) {
        return best_format;
      } else {
#ifndef NDEBUG
        LOG_INFO << "The single channel image format does not support linear "
                    "access, use the 4-channel format instead";
#endif  // DEBUG
        return high_precision ? intl::Format::eR16G16B16A16Sfloat
                              : intl::Format::eR8G8B8A8Unorm;
      }
    }

    case base::image::kRgbaImageChannel:
      return high_precision ? intl::Format::eR16G16B16A16Sfloat
                            : intl::Format::eR8G8B8A8Unorm;

    default:
      FATAL(std::format(
          "Number of channels can only be 1 or 4, while {} provided", channel));
  }
}

const std::vector<intl::Format>& GetSupportedDepthStencilFormats() {
  static const auto* formats = new std::vector<intl::Format>{
      intl::Format::eD24UnormS8Uint, intl::Format::eD32SfloatS8Uint};
  return *formats;
}

intl::Format ChooseDepthStencilImageFormat(const Context& context) {
  const auto format = FindImageFormatWithFeature(
      context, GetSupportedDepthStencilFormats(),
      intl::FormatFeatureFlagBits::eDepthStencilAttachment);
  ASSERT_HAS_VALUE(format, "Failed to find depth stencil image format");
  return format.value();
}

struct AllocatedImage {
  intl::Image handle;
  VmaAllocation allocation = VK_NULL_HANDLE;
};

// Creates an image and allocates its device memory via VMA.
AllocatedImage CreateImage(
    const Context& context, intl::ImageCreateFlags create_flags,
    intl::Format format, const intl::Extent2D& extent, uint32_t mip_levels,
    uint32_t layer_count, intl::SampleCountFlagBits sample_count,
    intl::ImageUsageFlags usage_flags,
    const std::vector<uint32_t>& unique_queue_family_indices) {
  const auto image_create_info =
      intl::ImageCreateInfo{}
          .setFlags(create_flags)
          .setImageType(intl::ImageType::e2D)
          .setFormat(format)
          .setExtent({extent.width, extent.height, /*depth=*/1})
          .setMipLevels(mip_levels)
          .setArrayLayers(layer_count)
          .setSamples(sample_count)
          .setTiling(intl::ImageTiling::eOptimal)
          .setUsage(usage_flags)
          .setSharingMode(unique_queue_family_indices.size() > 1
                              ? intl::SharingMode::eConcurrent
                              : intl::SharingMode::eExclusive)
          .setQueueFamilyIndices(unique_queue_family_indices)
          .setInitialLayout(intl::ImageLayout::eUndefined);
  const VmaAllocationCreateInfo alloc_info{
      .usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
  };
  VkImage raw_image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  ASSERT_SUCCESS(static_cast<intl::Result>(vmaCreateImage(
                     context.vma_allocator(),
                     &static_cast<const VkImageCreateInfo&>(image_create_info),
                     &alloc_info, &raw_image, &allocation,
                     /*pAllocationInfo=*/nullptr)),
                 "Failed to create VMA image");
  return {.handle = raw_image, .allocation = allocation};
}

int ComputeMipLevels(const glm::ivec2& extent, bool generate_mipmaps) {
  if (!generate_mipmaps) {
    return kSingleMipLevel;
  }
  const int max_dim = std::max(extent.x, extent.y);
  return static_cast<int>(std::floor(std::log2(max_dim))) + 1;
}

// Uploads 'host_image' pixel data into 'target' via a staging buffer and
// generates mipmaps if target.mip_levels() > 1.
void UploadImageData(const Context& context, const OwnedImage& target,
                     const base::Image& host_image,
                     std::span<const ImageUsage> usages) {
  const std::vector<const void*> data_ptrs = host_image.GetDataPtrs();
  const glm::ivec2 extent = host_image.extent();
  const size_t layer_size =
      static_cast<size_t>(extent.x) * extent.y * host_image.channel();
  const uint32_t layer_count = CAST_TO_UINT(data_ptrs.size());
  const intl::DeviceSize total_size = layer_size * layer_count;

  const buffer::AllocatedBuffer staging =
      buffer::CreateStagingBuffer(context, total_size);
  std::vector<ir::Buffer::CopyInfo> copy_infos;
  copy_infos.reserve(layer_count);
  for (uint32_t layer = 0; layer < layer_count; ++layer) {
    copy_infos.push_back({
        .data = data_ptrs[layer],
        .size = layer_size,
        .offset = layer_size * layer,
    });
  }
  buffer::CopyHostDataToAllocation(context, staging.allocation, copy_infos);

  const uint32_t mip_levels = CAST_TO_UINT(target.mip_levels());
  if (mip_levels > 1) {
    const auto format_props =
        context.physical_device()->getFormatProperties(target.format());
    ASSERT_TRUE(
        static_cast<bool>(
            format_props.optimalTilingFeatures &
            intl::FormatFeatureFlagBits::eSampledImageFilterLinear),
        "Image format does not support linear blitting for mipmap generation");
  }

  intl::ImageLayout target_layout = intl::ImageLayout::eShaderReadOnlyOptimal;
  intl::PipelineStageFlags2 dst_stage =
      intl::PipelineStageFlagBits2::eFragmentShader;
  intl::AccessFlags2 dst_access = intl::AccessFlagBits2::eShaderRead;
  if (!usages.empty()) {
    const intl::ImageLayout usage_layout =
        image::GetImageLayout(usages.front());
    if (usage_layout != intl::ImageLayout::eUndefined) {
      target_layout = usage_layout;
    }
    const intl::PipelineStageFlags2 usage_stage =
        image::GetPipelineStageFlags(usages.front());
    if (usage_stage != intl::PipelineStageFlagBits2::eNone) {
      dst_stage = usage_stage;
    }
    const intl::AccessFlags2 usage_access =
        image::GetAccessFlags(usages.front());
    if (usage_access != intl::AccessFlags2{}) {
      dst_access = usage_access;
    }
  }

  const intl::ImageAspectFlags aspect_mask = target.GetAspectFlags();
  context.RunOneTimeGraphicsCommand([&](intl::CommandBuffer cmd) {
    image::TransitionImageLayout(cmd, *target, aspect_mask,
                                 /*base_mip_level=*/0, mip_levels, layer_count,
                                 intl::ImageLayout::eUndefined,
                                 intl::ImageLayout::eTransferDstOptimal,
                                 intl::PipelineStageFlagBits2::eTopOfPipe, {},
                                 intl::PipelineStageFlagBits2::eTransfer,
                                 intl::AccessFlagBits2::eTransferWrite);

    const auto copy_region =
        intl::BufferImageCopy{}
            .setBufferOffset(0)
            .setBufferRowLength(0)
            .setBufferImageHeight(0)
            .setImageSubresource({aspect_mask, /*mipLevel=*/0,
                                  /*baseArrayLayer=*/0, layer_count})
            .setImageOffset({0, 0, 0})
            .setImageExtent(
                {CAST_TO_UINT(extent.x), CAST_TO_UINT(extent.y), 1});
    cmd.copyBufferToImage(staging.handle, *target,
                          intl::ImageLayout::eTransferDstOptimal, copy_region);

    if (mip_levels > 1) {
      int mip_width = extent.x;
      int mip_height = extent.y;
      for (uint32_t level = 1; level < mip_levels; ++level) {
        image::TransitionImageLayout(cmd, *target, aspect_mask,
                                     /*base_mip_level=*/level - 1,
                                     /*level_count=*/1, layer_count,
                                     intl::ImageLayout::eTransferDstOptimal,
                                     intl::ImageLayout::eTransferSrcOptimal,
                                     intl::PipelineStageFlagBits2::eTransfer,
                                     intl::AccessFlagBits2::eTransferWrite,
                                     intl::PipelineStageFlagBits2::eTransfer,
                                     intl::AccessFlagBits2::eTransferRead);

        const int next_width = std::max(mip_width / 2, 1);
        const int next_height = std::max(mip_height / 2, 1);
        const auto blit =
            intl::ImageBlit{}
                .setSrcSubresource(
                    {aspect_mask, level - 1, /*baseArrayLayer=*/0, layer_count})
                .setSrcOffsets({intl::Offset3D{0, 0, 0},
                                intl::Offset3D{mip_width, mip_height, 1}})
                .setDstSubresource(
                    {aspect_mask, level, /*baseArrayLayer=*/0, layer_count})
                .setDstOffsets({intl::Offset3D{0, 0, 0},
                                intl::Offset3D{next_width, next_height, 1}});
        cmd.blitImage(*target, intl::ImageLayout::eTransferSrcOptimal, *target,
                      intl::ImageLayout::eTransferDstOptimal, blit,
                      intl::Filter::eLinear);

        image::TransitionImageLayout(
            cmd, *target, aspect_mask, /*base_mip_level=*/level - 1,
            /*level_count=*/1, layer_count,
            intl::ImageLayout::eTransferSrcOptimal, target_layout,
            intl::PipelineStageFlagBits2::eTransfer,
            intl::AccessFlagBits2::eTransferRead, dst_stage, dst_access);

        mip_width = next_width;
        mip_height = next_height;
      }
    }

    image::TransitionImageLayout(
        cmd, *target, aspect_mask, /*base_mip_level=*/mip_levels - 1,
        /*level_count=*/1, layer_count, intl::ImageLayout::eTransferDstOptimal,
        target_layout, intl::PipelineStageFlagBits2::eTransfer,
        intl::AccessFlagBits2::eTransferWrite, dst_stage, dst_access);
  });

  buffer::DestroyBuffer(context, staging);
}

intl::ImageView CreateImageViewForImage(const Context& context,
                                        intl::Image vk_image,
                                        const Image& image) {
  return image::CreateImageView(context, vk_image, image.GetViewType(),
                                image.format(), image.GetAspectFlags(),
                                CAST_TO_UINT(image.mip_levels()),
                                CAST_TO_UINT(image.GetNumLayers()));
}

std::vector<uint32_t> CollectUniqueQueueFamilyIndices(
    const Context& context, std::span<const ImageUsage> usages,
    bool include_graphics_queue) {
  std::unordered_set<uint32_t> queue_family_indices_set;
  for (const ImageUsage& usage : usages) {
    const auto queue_family_index = image::GetQueueFamilyIndex(context, usage);
    if (queue_family_index.has_value()) {
      queue_family_indices_set.insert(queue_family_index.value());
    }
  }
  if (include_graphics_queue) {
    queue_family_indices_set.insert(
        context.physical_device().queue_family_indices().graphics);
  }
  ASSERT_NON_EMPTY(queue_family_indices_set,
                   "Cannot find any queue used for this image");
  return {queue_family_indices_set.begin(), queue_family_indices_set.end()};
}

}  // namespace

intl::ImageViewType Image::GetViewType() const {
  switch (layer_type()) {
    case LayerType::kSingle:
      return intl::ImageViewType::e2D;
    case LayerType::kCubemap:
      return intl::ImageViewType::eCube;
  }
}

intl::ImageAspectFlags Image::GetAspectFlags() const {
  const std::span<const intl::Format> formats_span{
      GetSupportedDepthStencilFormats()};
  if (base::util::Contains(formats_span, format())) {
    return intl::ImageAspectFlagBits::eDepth |
           intl::ImageAspectFlagBits::eStencil;
  } else {
    return intl::ImageAspectFlagBits::eColor;
  }
}

std::unique_ptr<OwnedImage> OwnedImage::CreateColorImage(
    const SharedContext& context, std::string_view name,
    const base::Image::Dimension& dimension,
    MultisamplingMode multisampling_mode, bool high_precision,
    std::span<const ImageUsage> usages) {
  const intl::Format format = ChooseColorImageFormat(
      *context, dimension.channel, high_precision, usages);
  return std::unique_ptr<OwnedImage>(
      new OwnedImage(context, name, LayerType::kSingle, dimension.extent(),
                     kSingleMipLevel, format, multisampling_mode, usages));
}

std::unique_ptr<OwnedImage> OwnedImage::CreateColorImage(
    const SharedContext& context, std::string_view name,
    const Image& match_target, MultisamplingMode multisampling_mode,
    std::span<const ImageUsage> usages) {
  return std::unique_ptr<OwnedImage>(new OwnedImage(
      context, name, LayerType::kSingle,
      {match_target.width(), match_target.height()}, kSingleMipLevel,
      match_target.format(), multisampling_mode, usages));
}

std::unique_ptr<OwnedImage> OwnedImage::CreateColorImage(
    const SharedContext& context, std::string_view name,
    const base::Image& image, bool generate_mipmaps,
    std::span<const ImageUsage> usages) {
  const intl::Format format = ChooseColorImageFormat(
      *context, image.channel(), /*high_precision=*/false, usages);
  const int mip_levels = ComputeMipLevels(image.extent(), generate_mipmaps);
  intl::ImageUsageFlags extra_usage_flags =
      intl::ImageUsageFlagBits::eTransferDst;
  if (mip_levels > 1) {
    extra_usage_flags |= intl::ImageUsageFlagBits::eTransferSrc;
  }
  auto result = std::unique_ptr<OwnedImage>(new OwnedImage(
      context, name, image.type(), image.extent(), mip_levels, format,
      MultisamplingMode::kNone, usages, extra_usage_flags,
      /*include_graphics_queue=*/true));
  UploadImageData(*context, *result, image, usages);
  return result;
}

std::unique_ptr<OwnedImage> OwnedImage::CreateDepthStencilImage(
    const SharedContext& context, std::string_view name,
    const glm::ivec2& extent, MultisamplingMode multisampling_mode,
    std::span<const ImageUsage> usages) {
  return std::unique_ptr<OwnedImage>(new OwnedImage(
      context, name, LayerType::kSingle, extent, kSingleMipLevel,
      GetDepthStencilFormat(*context), multisampling_mode, usages));
}

intl::Format OwnedImage::GetDepthStencilFormat(const Context& context) {
  return ChooseDepthStencilImageFormat(context);
}

std::unique_ptr<OwnedImage> OwnedImage::CreateTransient(
    const SharedContext& context, std::string_view name,
    const glm::ivec2& extent, intl::Format format,
    MultisamplingMode multisampling_mode, intl::ImageUsageFlags usage_flags) {
  const auto& queue_families =
      context->physical_device().queue_family_indices();
  const std::unordered_set<uint32_t> queue_set{queue_families.graphics,
                                               queue_families.compute};
  const std::vector<uint32_t> unique_queues{queue_set.begin(), queue_set.end()};
  return std::unique_ptr<OwnedImage>(
      new OwnedImage(context, name, LayerType::kSingle, extent, kSingleMipLevel,
                     format, multisampling_mode, usage_flags, unique_queues));
}

OwnedImage::OwnedImage(const SharedContext& context, std::string_view name,
                       LayerType layer_type, const glm::ivec2& extent,
                       int mip_levels, intl::Format format,
                       ir::MultisamplingMode multisampling_mode,
                       intl::ImageUsageFlags usage_flags,
                       const std::vector<uint32_t>& unique_queue_family_indices)
    : WithSharedContext{context},
      Image{name,
            layer_type,
            extent,
            mip_levels,
            format,
            context_->physical_device().sample_count(multisampling_mode)} {
  intl::ImageCreateFlags create_flags;
  if (layer_type == LayerType::kCubemap) {
    create_flags |= intl::ImageCreateFlagBits::eCubeCompatible;
  }

  const AllocatedImage allocated =
      CreateImage(*context_, create_flags, format, util::ToExtent(extent),
                  mip_levels, CAST_TO_UINT(GetNumLayers()), sample_count(),
                  usage_flags, unique_queue_family_indices);
  image_ = allocated.handle;
  allocation_ = allocated.allocation;
  image_view_ = CreateImageViewForImage(*context_, image_, *this);
}

OwnedImage::OwnedImage(const SharedContext& context, std::string_view name,
                       LayerType layer_type, const glm::ivec2& extent,
                       int mip_levels, intl::Format format,
                       ir::MultisamplingMode multisampling_mode,
                       std::span<const ir::ImageUsage> usages,
                       intl::ImageUsageFlags extra_usage_flags,
                       bool include_graphics_queue)
    : OwnedImage{context,
                 name,
                 layer_type,
                 extent,
                 mip_levels,
                 format,
                 multisampling_mode,
                 image::GetImageUsageFlags(usages) | extra_usage_flags,
                 CollectUniqueQueueFamilyIndices(*context, usages,
                                                 include_graphics_queue)} {}

OwnedImage::~OwnedImage() {
  if (image_view_ == intl::ImageView{} && allocation_ == VK_NULL_HANDLE) {
    return;
  }
  const intl::ImageView image_view = image_view_;
  const intl::Image image = image_;
  const VmaAllocation allocation = allocation_;
  context_->AddReleaseExpiredResourceOp([image_view, image,
                                         allocation](const Context& context) {
    if (image_view != intl::ImageView{}) {
      context.device()->destroyImageView(image_view, *context.host_allocator());
    }
    if (allocation != VK_NULL_HANDLE) {
      vmaDestroyImage(context.vma_allocator(), static_cast<VkImage>(image),
                      allocation);
    }
  });
}

Sampler::Sampler(const SharedContext& context,
                 const ir::SamplerDescriptor& config, int mip_levels)
    : WithSharedContext{context} {
  const intl::Filter filter = type::ConvertFilterType(config.filter_type);
  const intl::SamplerAddressMode address_mode =
      type::ConvertSamplerAddressMode(config.address_mode);
  const intl::SamplerMipmapMode mipmap_mode =
      config.filter_type == ir::FilterType::kNearest
          ? intl::SamplerMipmapMode::eNearest
          : intl::SamplerMipmapMode::eLinear;
  const float max_anisotropy =
      context_->physical_device()->getProperties().limits.maxSamplerAnisotropy;
  const auto create_info =
      intl::SamplerCreateInfo{}
          .setMagFilter(filter)
          .setMinFilter(filter)
          .setMipmapMode(mipmap_mode)
          .setAddressModeU(address_mode)
          .setAddressModeV(address_mode)
          .setAddressModeW(address_mode)
          .setAnisotropyEnable(intl::True)
          .setMaxAnisotropy(max_anisotropy)
          .setCompareOp(intl::CompareOp::eAlways)
          .setMinLod(0.0f)
          .setMaxLod(static_cast<float>(std::max(mip_levels, 1)))
          .setBorderColor(intl::BorderColor::eIntOpaqueBlack)
          .setUnnormalizedCoordinates(intl::False);
  sampler_ = vk_device().createSampler(create_info, vk_host_allocator());
}

Sampler::~Sampler() { context_->DeviceDestroy(sampler_); }

}  // namespace lighter::renderer::vk
