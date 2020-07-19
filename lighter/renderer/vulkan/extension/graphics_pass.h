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
  GraphicsPassBuilder(SharedBasicContext context, int num_subpasses)
      : BasePass{num_subpasses}, context_{std::move(FATAL_IF_NULL(context))} {}

  // This class is neither copyable nor movable.
  GraphicsPassBuilder(const GraphicsPassBuilder&) = delete;
  GraphicsPassBuilder& operator=(const GraphicsPassBuilder&) = delete;

  std::unique_ptr<GraphicsPass> Build() const;

 private:
  friend class GraphicsPass;

  // Pointer to context.
  const SharedBasicContext context_;
};

class GraphicsPass {
 public:
  using AttachmentIndexMap = absl::flat_hash_map<const Image*, int>;

  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;

 private:
  friend std::unique_ptr<GraphicsPass> GraphicsPassBuilder::Build() const;

  GraphicsPass(const GraphicsPassBuilder& graphics_pass_builder);

  void SetAttachments(const GraphicsPassBuilder& graphics_pass_builder,
                      RenderPassBuilder* render_pass_builder);

  const AttachmentIndexMap attachment_index_map_;

  std::unique_ptr<RenderPass> render_pass_;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H */
