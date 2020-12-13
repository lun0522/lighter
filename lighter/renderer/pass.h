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
#include <vector>

#include "lighter/common/util.h"
#include "lighter/renderer/image.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/type.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/span.h"

namespace lighter {
namespace renderer {

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

class BasePassDescriptor {
 public:
  struct ImageAndUsage {
    const DeviceImage& image;
    const ImageUsage& usage;
  };

  // This class provides copy constructor and move constructor.
  BasePassDescriptor(BasePassDescriptor&&) noexcept = default;
  BasePassDescriptor(const BasePassDescriptor&) = default;

  virtual ~BasePassDescriptor() = default;

 protected:
  // Maps subpasses where the image is used to its usage at that subpass. An
  // ordered map is used to look up the previous/next usage efficiently.
  using ImageUsageHistory = std::map<int, ImageUsage>;

  using ImageUsageHistoryMap = absl::flat_hash_map<const DeviceImage*,
                                                   ImageUsageHistory>;

  explicit BasePassDescriptor(absl::Span<const DeviceImage* const> images);

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

class GraphicsPassDescriptor : public BasePassDescriptor {
 public:
  struct LoadStoreOps {
    AttachmentLoadOp load_op;
    AttachmentStoreOp store_op;
  };

  struct ColorAttachment {
    ColorAttachment(const DeviceImage* image, const LoadStoreOps& color_ops)
        : image{*FATAL_IF_NULL(image)}, color_ops{color_ops} {}

    const DeviceImage& image;
    const LoadStoreOps& color_ops;
  };

  struct DepthStencilAttachment {
    DepthStencilAttachment(const DeviceImage* image,
                           const LoadStoreOps& depth_ops,
                           const LoadStoreOps& stencil_ops)
        : image{*FATAL_IF_NULL(image)}, depth_ops{depth_ops},
          stencil_ops{stencil_ops} {}

    const DeviceImage& image;
    const LoadStoreOps& depth_ops;
    const LoadStoreOps& stencil_ops;
  };

  struct MultisamplingResolve {
    const DeviceImage& source_image;
    const DeviceImage& target_image;
  };

  GraphicsPassDescriptor(
      absl::Span<const ColorAttachment> color_attachments,
      absl::Span<const DepthStencilAttachment> depth_stencil_attachments,
      absl::Span<const DeviceImage* const> other_images);

  // This class provides copy constructor and move constructor.
  GraphicsPassDescriptor(GraphicsPassDescriptor&&) noexcept = default;
  GraphicsPassDescriptor(const GraphicsPassDescriptor&) = default;

  GraphicsPassDescriptor& AddSubpass(
      absl::Span<const ImageAndUsage> images_and_usages) {
    return AddSubpass(images_and_usages, /*resolves=*/{});
  }
  GraphicsPassDescriptor& AddSubpass(
      absl::Span<const ImageAndUsage> images_and_usages,
      absl::Span<const MultisamplingResolve> resolves);

 private:
  using LoadStoreOpsMap = absl::flat_hash_map<const DeviceImage*, LoadStoreOps>;

  LoadStoreOpsMap color_ops_map_;
  LoadStoreOpsMap depth_ops_map_;
  LoadStoreOpsMap stencil_ops_map_;
  std::vector<std::vector<MultisamplingResolve>> multisampling_resolves_;
};

class ComputePassDescriptor : public BasePassDescriptor {
 public:
  // This class provides copy constructor and move constructor.
  ComputePassDescriptor(ComputePassDescriptor&&) noexcept = default;
  ComputePassDescriptor(const ComputePassDescriptor&) = default;
};

} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PASS_H */
