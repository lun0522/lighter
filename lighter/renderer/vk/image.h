#ifndef LIGHTER_RENDERER_VK_IMAGE_H
#define LIGHTER_RENDERER_VK_IMAGE_H

#include <vk_mem_alloc.h>

#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <string_view>

#include "lighter/base/image.h"
#include "lighter/base/util.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

// Vulkan extension of ir::Image exposing VkFormat, sample count, and
// VkImageView.
class Image : public ir::Image {
 public:
  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  static const Image& Cast(const ir::Image& image) {
    return static_cast<const Image&>(image);
  }

  intl::ImageViewType GetViewType() const;

  intl::ImageAspectFlags GetAspectFlags() const;

  virtual intl::ImageView image_view() const = 0;

  virtual intl::Image vk_image() const = 0;

  // Accessors.
  intl::Extent2D extent() const {
    return util::CreateExtent(width(), height());
  }
  intl::Format format() const { return format_; }
  intl::SampleCountFlagBits sample_count() const { return sample_count_; }

 protected:
  Image(std::string_view name, LayerType layer_type, const glm::ivec2& extent,
        int mip_levels, intl::Format format,
        intl::SampleCountFlagBits sample_count)
      : ir::Image{name, layer_type, extent, mip_levels},
        format_{format},
        sample_count_{sample_count} {}

 private:
  const intl::Format format_;
  const intl::SampleCountFlagBits sample_count_;
};

// VMA-allocated GPU image owning its VkImage, memory allocation, and
// VkImageView.
class OwnedImage : public WithSharedContext, public Image {
 public:
  static std::unique_ptr<OwnedImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const base::Image::Dimension& dimension,
      ir::MultisamplingMode multisampling_mode, bool high_precision,
      std::span<const ir::ImageUsage> usages);

  static std::unique_ptr<OwnedImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const Image& match_target, ir::MultisamplingMode multisampling_mode,
      std::span<const ir::ImageUsage> usages);

  static std::unique_ptr<OwnedImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const base::Image& image, bool generate_mipmaps,
      std::span<const ir::ImageUsage> usages);

  static std::unique_ptr<OwnedImage> CreateDepthStencilImage(
      const SharedContext& context, std::string_view name,
      const glm::ivec2& extent, ir::MultisamplingMode multisampling_mode,
      std::span<const ir::ImageUsage> usages);

  static intl::Format GetDepthStencilFormat(const Context& context);

  static std::unique_ptr<OwnedImage> CreateTransient(
      const SharedContext& context, std::string_view name,
      const glm::ivec2& extent, intl::Format format,
      ir::MultisamplingMode multisampling_mode,
      intl::ImageUsageFlags usage_flags);

  // This class is neither copyable nor movable.
  OwnedImage(const OwnedImage&) = delete;
  OwnedImage& operator=(const OwnedImage&) = delete;

  ~OwnedImage() override;

  static const OwnedImage& Cast(const ir::Image& image) {
    return static_cast<const OwnedImage&>(image);
  }

  // Accessors.
  intl::ImageView image_view() const override { return image_view_; }
  intl::Image vk_image() const override { return image_; }

  // Overloads.
  intl::Image operator*() const { return image_; }

 private:
  OwnedImage(const SharedContext& context, std::string_view name,
             LayerType layer_type, const glm::ivec2& extent, int mip_levels,
             intl::Format format, ir::MultisamplingMode multisampling_mode,
             intl::ImageUsageFlags usage_flags,
             const std::vector<uint32_t>& unique_queue_family_indices);

  OwnedImage(const SharedContext& context, std::string_view name,
             LayerType layer_type, const glm::ivec2& extent, int mip_levels,
             intl::Format format, ir::MultisamplingMode multisampling_mode,
             std::span<const ir::ImageUsage> usages,
             intl::ImageUsageFlags extra_usage_flags = {},
             bool include_graphics_queue = false);

  // Opaque image objects.
  intl::Image image_;
  intl::ImageView image_view_;

  // VMA allocation handle.
  VmaAllocation allocation_ = VK_NULL_HANDLE;
};

// Wraps VkSampler configured from ir::SamplerDescriptor.
class Sampler : public WithSharedContext {
 public:
  explicit Sampler(const SharedContext& context,
                   const ir::SamplerDescriptor& config, int mip_levels = 1);

  // This class is neither copyable nor movable.
  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  ~Sampler();

  // Overloads.
  intl::Sampler operator*() const { return sampler_; }

 private:
  intl::Sampler sampler_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_IMAGE_H
