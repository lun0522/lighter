//
//  image_usage.h
//
//  Created by Pujun Lun on 10/11/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IMAGE_USAGE_H
#define LIGHTER_RENDERER_IMAGE_USAGE_H

#include <algorithm>
#include <map>
#include <optional>

#include "lighter/common/util.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/str_format.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer {

// Describes how we would use an image.
class ImageUsage {
 public:
  // TODO: Consider VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT.
  // Usage types of images that we can handle.
  enum class UsageType {
    // Don't care about the content stored in the image.
    kDontCare,
    // Color attachment.
    kRenderTarget,
    // Depth stencil attachment.
    kDepthStencil,
    // A multisample image resolves to a single sample image.
    kMultisampleResolve,
    // Presented to screen.
    kPresentation,
    // Linearly accessed.
    kLinearAccess,
    // Only the value stored at the same pixel is accessed.
    kInputAttachment,
    // Sampled as texture.
    kSample,
    // Used for transferring image data within the device, e.g. blitting one
    // image to another.
    kTransfer,
  };

  // Convenience function to return usage for images sampled as textures in
  // fragment shaders.
  static ImageUsage GetSampledInFragmentShaderUsage() {
    return ImageUsage{UsageType::kSample, AccessType::kReadOnly,
                      AccessLocation::kFragmentShader};
  }

  // Convenience function to return usage for images used as render targets.
  static ImageUsage GetRenderTargetUsage(int attachment_location) {
    return ImageUsage{UsageType::kRenderTarget, AccessType::kReadWrite,
                      AccessLocation::kOther, attachment_location};
  }

  // Convenience function to return usage for images that we resolve multisample
  // images to.
  static ImageUsage GetMultisampleResolveTargetUsage() {
    return ImageUsage{UsageType::kMultisampleResolve, AccessType::kWriteOnly,
                      AccessLocation::kOther};
  }

  // Convenience function to return usage for images used as depth stencil
  // attachment.
  static ImageUsage GetDepthStencilUsage(AccessType access_type) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type");
    return ImageUsage{UsageType::kDepthStencil, access_type,
                      AccessLocation::kOther};
  }

  // Convenience function to return usage for images to be presented to screen.
  static ImageUsage GetPresentationUsage() {
    return ImageUsage{UsageType::kPresentation, AccessType::kReadOnly,
                      AccessLocation::kOther};
  }

  // Convenience function to return usage for images used as input attachments.
  static ImageUsage GetInputAttachmentUsage() {
    return ImageUsage{UsageType::kInputAttachment, AccessType::kReadOnly,
                      AccessLocation::kFragmentShader};
  }

  // Convenience function to return usage for images linearly accessed in
  // compute shaders.
  static ImageUsage GetLinearAccessInComputeShaderUsage(
      AccessType access_type) {
    ASSERT_FALSE(access_type == AccessType::kDontCare,
                 "Must specify access type");
    return ImageUsage{UsageType::kLinearAccess, access_type,
                      AccessLocation::kComputeShader};
  }

  explicit ImageUsage() : ImageUsage{UsageType::kDontCare,
                                     AccessType::kDontCare,
                                     AccessLocation::kDontCare} {}

  // Returns true if any of 'usages' is UsageType::kLinearAccess.
  static bool IsLinearAccessed(absl::Span<const ImageUsage> usages) {
    return std::any_of(usages.begin(), usages.end(),
                       [](const ImageUsage& usage) {
                         return usage.usage_type() == UsageType::kLinearAccess;
                       });
  }

  // Overrides.
  bool operator==(const ImageUsage& other) const {
    return usage_type_ == other.usage_type_ &&
           access_type_ == other.access_type_ &&
           access_location_ == other.access_location_;
  }

  // Accessors.
  UsageType usage_type() const { return usage_type_; }
  AccessType access_type() const { return access_type_; }
  AccessLocation access_location() const { return access_location_; }
  int attachment_location() const { return attachment_location_.value(); }

 private:
  // We make this constructor private so that the user can only construct the
  // default usage or use static methods to construct usages that are guaranteed
  // to be valid.
  ImageUsage(UsageType usage_type, AccessType access_type,
             AccessLocation access_location,
             std::optional<int> attachment_location = std::nullopt)
      : usage_type_{usage_type}, access_type_{access_type},
        access_location_{access_location},
        attachment_location_{attachment_location} {}

  UsageType usage_type_;
  AccessType access_type_;
  AccessLocation access_location_;
  std::optional<int> attachment_location_;
};

// TODO: Remove.
// This class holds usages of an image in subpasses of a compute pass or
// graphics pass. We assume that an image can only have one usage at a subpass.
class ImageUsageHistory {
 public:
  // Maps subpass to the name of image that will resolve to this image at that
  // subpass.
  using MultisampleResolveSourceMap = absl::flat_hash_map<int, std::string>;

  explicit ImageUsageHistory(const ImageUsage& initial_usage)
      : initial_usage_{initial_usage} {}

  // This constructor should be used only if the image has not been constructed
  // yet. In that case, the user should add all usages throughout the entire
  // lifetime of the image to this history.
  explicit ImageUsageHistory() = default;

  // This class is only movable.
  ImageUsageHistory(ImageUsageHistory&&) noexcept = default;
  ImageUsageHistory& operator=(ImageUsageHistory&&) noexcept = default;

  // Specifies the usage at 'subpass'.
  ImageUsageHistory& AddUsage(int subpass, const ImageUsage& usage) {
    const auto did_insert =
        usage_at_subpass_map_.insert({subpass, usage}).second;
    ASSERT_TRUE(
        did_insert,
        absl::StrFormat("Already specified usage for subpass %d", subpass));
    return *this;
  }

  // Specifies the same usage for all subpasses in range
  // ['subpass_start', 'subpass_end'].
  ImageUsageHistory& AddUsage(int subpass_start, int subpass_end,
                              const ImageUsage& usage) {
    ASSERT_TRUE(subpass_start <= subpass_end,
                absl::StrFormat("Invalid range [%d, %d]",
                                subpass_start, subpass_end));
    for (int subpass = subpass_start; subpass <= subpass_end; ++subpass) {
      AddUsage(subpass, usage);
    }
    return *this;
  }

  // Specifies that the multisample image with 'source_image_name' will resolve
  // to this image at 'subpass'.
  ImageUsageHistory& AddMultisampleResolveSource(
      int subpass, std::string_view source_image_name){
    const auto iter =
        resolve_source_map_.insert({subpass, std::string{source_image_name}});
    ASSERT_TRUE(iter.second,
                absl::StrFormat("Already specified source for subpass %d",
                                subpass));
    return AddUsage(subpass, ImageUsage::GetMultisampleResolveTargetUsage());
  }

  // Specifies the usage after this pass. This is optional. It should be called
  // only if the user wants to explicitly transition the image layout to prepare
  // for later operations.
  ImageUsageHistory& SetFinalUsage(const ImageUsage& usage) {
    ASSERT_NO_VALUE(final_usage_, "Already specified final usage");
    final_usage_ = usage;
    return *this;
  }

  // Returns all usages at all subpasses, including the initial and final usages
  // if specified. Note that this may contain duplicates.
  std::vector<ImageUsage> GetAllUsages() const {
    const int num_usages = 1 + static_cast<int>(usage_at_subpass_map_.size()) +
                           (final_usage_.has_value() ? 1 : 0);
    std::vector<ImageUsage> usages;
    usages.reserve(num_usages);
    usages.push_back(initial_usage_);
    for (const auto& pair : usage_at_subpass_map_) {
      usages.push_back(pair.second);
    }
    if (final_usage_.has_value()) {
      usages.push_back(final_usage_.value());
    }
    return usages;
  }

  // Accessors.
  const std::map<int, ImageUsage>& usage_at_subpass_map() const {
    return usage_at_subpass_map_;
  }
  const ImageUsage& initial_usage() const { return initial_usage_; }
  const std::optional<ImageUsage>& final_usage() const { return final_usage_; }
  const MultisampleResolveSourceMap& multisample_resolve_source_map() const {
    return resolve_source_map_;
  }

 private:
  // Maps subpasses where the image is used to its usage at that subpass. An
  // ordered map is used to look up the previous/next usage efficiently.
  std::map<int, ImageUsage> usage_at_subpass_map_;

  // Usage of image before this pass.
  ImageUsage initial_usage_;

  // Usage of image after this pass.
  std::optional<ImageUsage> final_usage_;

  // Records which images will resolve to this image at which subpasses.
  MultisampleResolveSourceMap resolve_source_map_;
};

// TODO: Remove.
// This class tracks usages of multiple images. Each image should have a unique
// name as identifier.
class ImageUsageTracker {
 public:
  ImageUsageTracker() = default;

  // This class is neither copyable nor movable.
  ImageUsageTracker(const ImageUsageTracker&) = delete;
  ImageUsageTracker& operator=(const ImageUsageTracker&) = delete;

  // Makes this tracker track the usage of image.
  ImageUsageTracker& TrackImage(std::string&& image_name,
                                const ImageUsage& current_usage) {
    ASSERT_FALSE(image_usage_map_.contains(image_name),
                 absl::StrFormat("Already tracking image with name '%s'",
                                 image_name));
    image_usage_map_.insert({std::move(image_name), current_usage});
    return *this;
  }

  ImageUsageTracker& TrackImage(const std::string& image_name,
                                const ImageUsage& current_usage) {
    return TrackImage(std::string{image_name}, current_usage);
  }

  // Returns true if the usage of image is being tracked.
  bool IsImageTracked(const std::string& image_name) const {
    return image_usage_map_.contains(image_name);
  }

  // Returns the current usage of image.
  const ImageUsage& GetUsage(const std::string& image_name) const {
    const auto iter = image_usage_map_.find(image_name);
    ASSERT_FALSE(iter == image_usage_map_.end(),
                 absl::StrFormat("Unrecognized image '%s'", image_name));
    return iter->second;
  }

  // Updates the current usage of image.
  ImageUsageTracker& UpdateUsage(const std::string& image_name,
                                 const ImageUsage& usage) {
    auto iter = image_usage_map_.find(image_name);
    ASSERT_FALSE(iter == image_usage_map_.end(),
                 absl::StrFormat("Unrecognized image '%s'", image_name));
    iter->second = usage;
    return *this;
  }

 private:
  // Maps the image name to the current usage of that image.
  absl::flat_hash_map<std::string, ImageUsage> image_usage_map_;
};

}  // namespace lighter::renderer

#endif  // LIGHTER_RENDERER_IMAGE_USAGE_H
