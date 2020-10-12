//
//  naive_render_pass.h
//
//  Created by Pujun Lun on 7/31/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H

#include <memory>
#include <string>

#include "lighter/common/util.h"
#include "lighter/renderer/image_usage.h"
#include "lighter/renderer/vulkan/extension/graphics_pass.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/strings/string_view.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vulkan {

// This class is built on the top of GraphicsPass, and is used to create render
// passes that may contain three types of subpasses, which differ in the
// readability and writability of the depth stencil attachment (if exists):
// 1. Opaque subpasses: the depth stencil attachment will be both readable and
//    writable, so that we can render opaque objects.
// 2. Transparent subpasses: the depth stencil attachment will be read-only,
//    so that we can render transparent objects.
// 3. Overlay subpasses: the depth stencil attachment will not be used. One of
//    the use cases is rendering texts on the top of the framebuffer.
// This class will create the image usage history for each attachment image
// according to the properties of these subpasses.
class NaiveRenderPass {
 public:
  // This class is used to infer the number of different types of subpasses of a
  // NaiveRenderPass.
  class SubpassConfig {
   public:
    SubpassConfig(int num_subpasses,
                  absl::optional<int> first_transparent_subpass,
                  absl::optional<int> first_overlay_subpass);

    // This class provides copy constructor and move constructor.
    SubpassConfig(SubpassConfig&&) noexcept = default;
    SubpassConfig(const SubpassConfig&) = default;

    // Returns the number of subpasses where the depth stencil attachment is
    // used.
    int num_subpasses_using_depth_stencil() const {
      return num_opaque_subpass_ + num_transparent_subpasses_;
    }

    // Returns the total number of subpasses.
    int num_subpasses() const {
      return num_subpasses_using_depth_stencil() + num_overlay_subpasses_;
    }

    // Returns whether the depth stencil attachment is used in any subpass.
    bool use_depth_stencil() const {
      return num_subpasses_using_depth_stencil() > 0;
    }

   private:
    friend class NaiveRenderPass;

    // Number of different types of subpasses. See class comments of
    // NaiveRenderPass.
    int num_opaque_subpass_ = 0;
    int num_transparent_subpasses_ = 0;
    int num_overlay_subpasses_ = 0;
  };

  // Stores the attachment info. 'attachment_index' will be populated after
  // NaiveRenderPass::CreateBuilder() is called.
  struct AttachmentConfig {
    AttachmentConfig(absl::string_view image_name,
                     absl::optional<int>* attachment_index)
        : image_name{image_name},
          attachment_index{*FATAL_IF_NULL(attachment_index)} {}

    // Sets whether to preserve the previous content of attachment image. By
    // default, the content of the attachment will be cleared at the beginning
    // of this render pass, and only the content of color attachment will be
    // preserved after this render pass.
    AttachmentConfig& set_load_store_ops(
        const GraphicsPass::AttachmentLoadStoreOps& ops) {
      load_store_ops = ops;
      return *this;
    }

    // Sets the usage of image after this render pass. This should be called
    // only if the user wants to explicitly transition the image layout to
    // prepare for operations after this render pass.
    AttachmentConfig& set_final_usage(const ImageUsage& usage) {
      final_usage = usage;
      return *this;
    }

    std::string image_name;
    absl::optional<int>& attachment_index;
    absl::optional<GraphicsPass::AttachmentLoadStoreOps> load_store_ops;
    absl::optional<ImageUsage> final_usage;
  };

  // Creates a RenderPassBuilder. If multisampling attachment is used, it will
  // be resolved to the color attachment at the last subpass. All attachments,
  // if used, must have image usages tracked by 'image_usage_tracker'.
  static std::unique_ptr<RenderPassBuilder> CreateBuilder(
      SharedBasicContext context, int num_framebuffers,
      const SubpassConfig& subpass_config,
      const AttachmentConfig& color_attachment_config,
      const AttachmentConfig* multisampling_attachment_config,
      const AttachmentConfig* depth_stencil_attachment_config,
      ImageUsageTracker& image_usage_tracker);
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H */
