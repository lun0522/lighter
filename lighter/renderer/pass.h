//
//  pass.h
//
//  Created by Pujun Lun on 10/16/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PASS_H
#define LIGHTER_RENDERER_PASS_H

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/pipeline.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/span.h"

namespace lighter::renderer {

class GraphicsPass {
 public:
  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;

  virtual ~GraphicsPass() = default;
};

class ComputePass {
 public:
  // This class is neither copyable nor movable.
  ComputePass(const ComputePass&) = delete;
  ComputePass& operator=(const ComputePass&) = delete;

  virtual ~ComputePass() = default;
};

class PassDescriptor {
 public:
  struct ImageAndUsage {
    ImageAndUsage(const DeviceImage* image, const ImageUsage& usage)
        : image{*FATAL_IF_NULL(image)}, usage{usage} {}

    const DeviceImage& image;
    ImageUsage usage;
  };

  // This class provides copy constructor and move constructor.
  PassDescriptor(PassDescriptor&&) noexcept = default;
  PassDescriptor(const PassDescriptor&) = default;

  virtual ~PassDescriptor() = default;

 protected:
  // Maps subpasses where the image is used to its usage at that subpass. An
  // ordered map is used to look up the previous/next usage efficiently.
  using ImageUsageHistory = std::map<int, ImageUsage>;

  using ImageUsageHistoryMap = absl::flat_hash_map<const DeviceImage*,
                                                   ImageUsageHistory>;

  explicit PassDescriptor(absl::Span<const DeviceImage* const> images);

  void AddSubpass(absl::Span<const ImageAndUsage> images_and_usages);

  // Accessors.
  int num_subpasses() const { return num_subpasses_; }
  const ImageUsageHistoryMap& image_usage_history_map() const {
    return image_usage_history_map_;
  }

 private:
  // Number of subpasses.
  int num_subpasses_;

  // Maps images to their usage history.
  ImageUsageHistoryMap image_usage_history_map_;
};

class GraphicsPassDescriptor : public PassDescriptor {
 public:
  struct LoadStoreOps {
    AttachmentLoadOp load_op;
    AttachmentStoreOp store_op;
  };

  using ColorLoadStoreOps = LoadStoreOps;

  struct DepthStencilLoadStoreOps {
    LoadStoreOps depth_ops;
    LoadStoreOps stencil_ops;
  };

  struct ColorAttachment {
    ColorAttachment(const DeviceImage* image, const LoadStoreOps& color_ops)
        : image{*FATAL_IF_NULL(image)}, load_store_ops{color_ops} {}

    const DeviceImage& image;
    ColorLoadStoreOps load_store_ops;
  };

  struct DepthStencilAttachment {
    DepthStencilAttachment(const DeviceImage* image,
                           const LoadStoreOps& depth_ops,
                           const LoadStoreOps& stencil_ops)
        : image{*FATAL_IF_NULL(image)},
          load_store_ops{depth_ops, stencil_ops} {}

    const DeviceImage& image;
    DepthStencilLoadStoreOps load_store_ops;
  };

  struct MultisamplingResolve {
    const DeviceImage& source_image;
    const DeviceImage& target_image;
  };

  GraphicsPassDescriptor(
      absl::Span<const ColorAttachment> color_attachments,
      absl::Span<const DepthStencilAttachment> depth_stencil_attachments,
      absl::Span<const DeviceImage* const> uniform_textures);

  // This class provides copy constructor and move constructor.
  GraphicsPassDescriptor(GraphicsPassDescriptor&&) noexcept = default;
  GraphicsPassDescriptor(const GraphicsPassDescriptor&) = default;

  GraphicsPassDescriptor& AddSubpass(
      absl::Span<const ImageAndUsage> images_and_usages,
      absl::Span<const GraphicsPipelineDescriptor> pipeline_descriptors) {
    return AddSubpass(images_and_usages, /*multisampling_resolves=*/{},
                      pipeline_descriptors);
  }
  GraphicsPassDescriptor& AddSubpass(
      absl::Span<const ImageAndUsage> images_and_usages,
      absl::Span<const MultisamplingResolve> multisampling_resolves,
      absl::Span<const GraphicsPipelineDescriptor> pipeline_descriptors);

 private:
  absl::flat_hash_map<const DeviceImage*, ColorLoadStoreOps> color_ops_map_;
  absl::flat_hash_map<const DeviceImage*, DepthStencilLoadStoreOps>
      depth_stencil_ops_map_;
  std::vector<std::vector<MultisamplingResolve>> multisampling_resolves_;
};

class ComputePassDescriptor : public PassDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  ComputePassDescriptor(ComputePassDescriptor&&) noexcept = default;
  ComputePassDescriptor(const ComputePassDescriptor&) = default;
};

}  // namespace lighter::renderer

#endif  // LIGHTER_RENDERER_PASS_H
