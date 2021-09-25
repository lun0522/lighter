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
  // This class is neither copyable nor movable.
  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  static const Image& Cast(const ir::Image& image) {
    return dynamic_cast<const Image&>(image);
  }

  // Accessors.
  intl::Extent2D extent() const {
    return util::CreateExtent(width(), height());
  }
  intl::Format format() const { return format_; }
  intl::SampleCountFlagBits sample_count() const { return sample_count_; }

 protected:
  Image(std::string_view name, Type type, const glm::ivec2& extent,
        int mip_levels, intl::Format format,
        intl::SampleCountFlagBits sample_count)
      : ir::Image{name, type, extent, mip_levels},
        format_{format}, sample_count_{sample_count} {}

 private:
  const intl::Format format_;

  const intl::SampleCountFlagBits sample_count_;
};

class SingleImage : public WithSharedContext, public Image {
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

 private:
  SingleImage(const SharedContext& context, std::string_view name, Type type,
              const glm::ivec2& extent, int mip_levels, intl::Format format,
              ir::MultisamplingMode multisampling_mode,
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
      : Image{name, Type::kSingle, extent, /*mip_levels=*/1, format,
              intl::SampleCountFlagBits::e1},
        images_{std::move(images)} {}

  // This class is neither copyable nor movable.
  MultiImage(const MultiImage&) = delete;
  MultiImage& operator=(const MultiImage&) = delete;

 private:
  // Opaque image objects.
  std::vector<intl::Image> images_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_IMAGE_H
