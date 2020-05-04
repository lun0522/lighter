//
//  render_pass_util.h
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H

#include <memory>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/types/optional.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// TODO: Make it a subclass of RenderPass.
// This render pass at least contains one color attachment. If any opaque or
// transparent subpass is used, a depth attachment will also be added.
// If multisampling is used (by passing a non-optional 'multisampling_mode' to
// the constructor), a multisample attachment will be added as well, and
// configured to resolve to the color attachment.
// Each subpass will wait for the previous subpass finish writing to the color
// attachment. See comments of SubpassConfig for details about subpasses.
class NaiveRenderPassBuilder {
 public:
  // The usage of color attachment at the end of this render pass.
  // TODO: This should be inferred from image usage.
  enum class ColorAttachmentFinalUsage {
    kPresentToScreen, kSampledAsTexture, kAccessedLinearly,
  };

  // Configures numbers of different kinds of subpasses. If multisampling is
  // enabled, the multisample attachment will be used as the rendering target in
  // opaque and transparent subpasses, instead of the color attachment.
  // It will be resolved to the color attachment in the last subpass.
  struct SubpassConfig {
    // If true, the first subpass will use the color attachment and the depth
    // attachment. The depth attachment should be set to writable in the
    // pipeline, so that all opaque objects can be rendered in one subpass.
    bool use_opaque_subpass;

    // These subpasses will use the color attachment and the depth attachment,
    // but the depth attachment should not be writable. They are used for
    // rendering transparent objects.
    int num_transparent_subpasses;

    // These subpasses will only use the color attachment. One of use cases is
    // rendering texts on the top of the frame.
    int num_overlay_subpasses;
  };

  // If 'present_to_screen' is false, we assume that the color attachment will
  // be read by other shaders.
  NaiveRenderPassBuilder(
      SharedBasicContext context, const SubpassConfig& subpass_config,
      int num_framebuffers, bool use_multisampling,
      ColorAttachmentFinalUsage color_attachment_final_usage);

  // This class is neither copyable nor movable.
  NaiveRenderPassBuilder(const NaiveRenderPassBuilder&) = delete;
  NaiveRenderPassBuilder& operator=(const NaiveRenderPassBuilder&) = delete;

  // Overloads.
  const RenderPassBuilder* operator->() const { return &builder_; }

  // Accessors.
  RenderPassBuilder* mutable_builder() { return &builder_; }
  int color_attachment_index() const { return 0; }
  bool has_depth_attachment() const {
    return depth_attachment_index_.has_value();
  }
  // The user is responsible for checking if the depth attachment is used.
  int depth_attachment_index() const {
    return depth_attachment_index_.value();
  }
  bool has_multisample_attachment() const {
    return multisample_attachment_index_.has_value();
  }
  // The user is responsible for checking if the multisample attachment is used.
  int multisample_attachment_index() const {
    return multisample_attachment_index_.value();
  }

 private:
  // Builder of render pass.
  RenderPassBuilder builder_;

  // Index of optional depth attachment.
  absl::optional<int> depth_attachment_index_;

  // Index of optional multisample attachment.
  absl::optional<int> multisample_attachment_index_;
};

// This render pass is used for the geometry pass of deferred shading. We assume
// that one depth attachment and several color attachments will be used. There
// is only one subpass in this render pass. The user can use
// NaiveRenderPassBuilder for the lighting pass.
class DeferredShadingRenderPassBuilder {
 public:
  DeferredShadingRenderPassBuilder(SharedBasicContext context,
                                   int num_framebuffers,
                                   int num_color_attachments);

  // This class is neither copyable nor movable.
  DeferredShadingRenderPassBuilder(
      const DeferredShadingRenderPassBuilder&) = delete;
  DeferredShadingRenderPassBuilder& operator=(
      const DeferredShadingRenderPassBuilder&) = delete;

  // Overloads.
  const RenderPassBuilder* operator->() const { return &builder_; }

  // Accessors.
  RenderPassBuilder* mutable_builder() { return &builder_; }
  int depth_attachment_index() const { return 0; }
  int color_attachments_index_base() const { return 1; }

 private:
  // Builder of render pass.
  RenderPassBuilder builder_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H */
