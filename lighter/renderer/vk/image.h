//
//  image.h
//
//  Created by Pujun Lun on 10/21/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_IMAGE_H
#define LIGHTER_RENDERER_VK_IMAGE_H

#include <memory>
#include <string_view>

#include "lighter/common/image.h"
#include "lighter/common/util.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/image_usage.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter::renderer::vk {

class Image : public ir::Image {
 public:
  enum class Type { kSingle, kMultiple };

  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  static const Image& Cast(const ir::Image& image) {
    return static_cast<const Image&>(image);
  }

  bool IsSingleImage() const { return type_ == Type::kSingle; }

  intl::ImageViewType GetViewType() const;

  intl::ImageAspectFlags GetAspectFlags() const;

  // Accessors.
  intl::Extent2D extent() const {
    return util::CreateExtent(width(), height());
  }
  Type type() const { return type_; }
  intl::Format format() const { return format_; }
  intl::SampleCountFlagBits sample_count() const { return sample_count_; }

 protected:
  Image(Type type, std::string_view name, LayerType layer_type,
        const glm::ivec2& extent, int mip_levels, intl::Format format,
        intl::SampleCountFlagBits sample_count)
      : ir::Image{name, layer_type, extent, mip_levels},
        type_{type}, format_{format}, sample_count_{sample_count} {}

 private:
  const Type type_;
  const intl::Format format_;
  const intl::SampleCountFlagBits sample_count_;
};

// "Single" means there is only one backing VkImage, which might be a cubemap.
class SingleImage : public WithSharedContext,
                    public Image {
 public:
  static std::unique_ptr<SingleImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const common::Image::Dimension& dimension,
      ir::MultisamplingMode multisampling_mode, bool high_precision,
      absl::Span<const ir::ImageUsage> usages);

  static std::unique_ptr<SingleImage> CreateColorImage(
      const SharedContext& context, std::string_view name,
      const common::Image& image, bool generate_mipmaps,
      absl::Span<const ir::ImageUsage> usages);

  static std::unique_ptr<SingleImage> CreateDepthStencilImage(
      const SharedContext& context, std::string_view name,
      const glm::ivec2& extent, ir::MultisamplingMode multisampling_mode,
      absl::Span<const ir::ImageUsage> usages);

  // This class is neither copyable nor movable.
  SingleImage(const SingleImage&) = delete;
  SingleImage& operator=(const SingleImage&) = delete;

  ~SingleImage() override;

  static const SingleImage& Cast(const ir::Image& image) {
    return static_cast<const SingleImage&>(image);
  }

  // Overloads.
  intl::Image operator*() const { return image_; }

 private:
  SingleImage(const SharedContext& context, std::string_view name,
              LayerType layer_type, const glm::ivec2& extent, int mip_levels,
              intl::Format format, ir::MultisamplingMode multisampling_mode,
              absl::Span<const ir::ImageUsage> usages);

  // Opaque image object.
  intl::Image image_;

  // TODO: Integrate VMA.
  // Opaque device memory object.
  intl::DeviceMemory device_memory_;
};

// A series of images that share the same size, format and usage, etc.
// Swapchain images are good examples of this type of image. We may need this
// when doing offscreen rendering and writing to the disk.
// TODO: Consider the case where we do own the image.
class MultiImage : public Image {
 public:
  MultiImage(std::string_view name, std::vector<intl::Image>&& images,
             const glm::ivec2& extent, intl::Format format)
      : Image{Type::kMultiple, name, LayerType::kSingle, extent,
              /*mip_levels=*/1, format, intl::SampleCountFlagBits::e1},
        images_{std::move(images)} {}

  // This class is neither copyable nor movable.
  MultiImage(const MultiImage&) = delete;
  MultiImage& operator=(const MultiImage&) = delete;

  static const MultiImage& Cast(const ir::Image& image) {
    return static_cast<const MultiImage&>(image);
  }

  // Accessors.
  int num_images() const { return images_.size(); }
  intl::Image image(int index) const { return images_.at(index); }

 private:
  // Opaque image objects.
  std::vector<intl::Image> images_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_IMAGE_H
