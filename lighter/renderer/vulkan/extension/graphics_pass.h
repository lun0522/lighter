//
//  graphics_pass.h
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H

#include <memory>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/base_pass.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/image_util.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/container/flat_hash_map.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// Forward declarations.
class GraphicsPass;

class GraphicsPassBuilder : public BasePass {
 public:
  GraphicsPassBuilder(SharedBasicContext context, int num_subpasses);

  // This class is neither copyable nor movable.
  GraphicsPassBuilder(const GraphicsPassBuilder&) = delete;
  GraphicsPassBuilder& operator=(const GraphicsPassBuilder&) = delete;

  GraphicsPassBuilder& AddMultisampleResolving(
      const Image& src_image, const Image& dst_image, int subpass);

  std::unique_ptr<GraphicsPass> Build();

 private:
  using AttachmentIndexMap = absl::flat_hash_map<const Image*, int>;

  using MultisamplingMap = absl::flat_hash_map<const Image*, const Image*>;

  friend class GraphicsPass;

  void RebuildAttachmentIndexMap();

  void SetAttachments();

  void SetSubpasses();

  void SetSubpassDependencies();

  image::Usage::UsageType GetImageUsageTypeForAllSubpasses(
      const image::UsageHistory& history) const;

  // Overrides.
  void ValidateImageUsageHistory(const image::UsageHistory& history) const
      override;

  // Pointer to context.
  const SharedBasicContext context_;

  AttachmentIndexMap attachment_index_map_;

  std::vector<MultisamplingMap> multisampling_at_subpass_maps_;

  std::unique_ptr<RenderPassBuilder> render_pass_builder_;
};

class GraphicsPass {
 public:
  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;

 private:
  using AttachmentIndexMap = GraphicsPassBuilder::AttachmentIndexMap;

  friend std::unique_ptr<GraphicsPass> GraphicsPassBuilder::Build();

  GraphicsPass(std::unique_ptr<RenderPass>&& render_pass,
               AttachmentIndexMap&& attachment_index_map)
      : render_pass_{std::move(render_pass)},
        attachment_index_map_{std::move(attachment_index_map)} {}

  const std::unique_ptr<RenderPass> render_pass_;

  const AttachmentIndexMap attachment_index_map_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H */
