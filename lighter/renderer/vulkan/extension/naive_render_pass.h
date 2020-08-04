//
//  naive_render_pass.h
//
//  Created by Pujun Lun on 7/31/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H

#include <memory>

#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/extension/attachment_info.h"
#include "lighter/renderer/vulkan/extension/image_util.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// TODO
class NaiveRenderPass {
 public:
  class SubpassConfig {
   public:
    SubpassConfig(int num_subpasses,
                  absl::optional<int> first_transparent_subpass,
                  absl::optional<int> first_overlay_subpass);

    // This class provides copy constructor and move constructor.
    SubpassConfig(SubpassConfig&&) noexcept = default;
    SubpassConfig(SubpassConfig&) = default;

    int num_subpasses_using_depth_stencil() const {
      return num_opaque_subpass_ + num_transparent_subpasses_;
    }

    int num_subpasses() const {
      return num_subpasses_using_depth_stencil() + num_overlay_subpasses_;
    }

    bool use_depth_stencil() const {
      return num_subpasses_using_depth_stencil() > 0;
    }

   private:
    friend class NaiveRenderPass;

    // Subpasses where the depth stencil attachment, if exists, will be both
    // readable and writable, so that we can render opaque objects.
    int num_opaque_subpass_ = 0;

    // Subpasses where the depth stencil attachment, if exists, will only be
    // readable, so that we can render transparent objects.
    int num_transparent_subpasses_ = 0;

    // Subpasses where the depth stencil attachment, if exists, will not be
    // used. One of use cases is rendering texts on the top of the framebuffer.
    int num_overlay_subpasses_ = 0;
  };

  struct AttachmentConfig {
    explicit AttachmentConfig(AttachmentInfo* attachment_info)
        : attachment_info{*FATAL_IF_NULL(attachment_info)} {}

    AttachmentConfig& set_load_store_ops(
        const GraphicsPass::AttachmentLoadStoreOps& ops) {
      load_store_ops = ops;
      return *this;
    }

    AttachmentConfig& set_final_usage(const image::Usage& usage) {
      final_usage = usage;
      return *this;
    }

    AttachmentInfo& attachment_info;
    absl::optional<GraphicsPass::AttachmentLoadStoreOps> load_store_ops;
    absl::optional<image::Usage> final_usage;
  };

  static std::unique_ptr<RenderPassBuilder> CreateBuilder(
      SharedBasicContext context, int num_framebuffers,
      const SubpassConfig& subpass_config,
      const AttachmentConfig& color_attachment_config,
      const AttachmentConfig* multisampling_attachment_config,
      const AttachmentConfig* depth_stencil_attachment_config,
      image::UsageTracker& image_usage_tracker);
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H */
