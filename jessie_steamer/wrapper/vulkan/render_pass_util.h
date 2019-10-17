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
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/types/optional.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// This render pass contains one color attachment and one depth attachment.
// If 'multisampling_mode' has value, a multisample attachment will be added as
// well, and configured to resolve to the color attachment.
// Each subpass will wait for the previous subpass finish writing to the color
// attachment. See comments of SubpassConfig for details about subpasses.
namespace naive_render_pass {

// Configures numbers of different kinds of subpasses.
// If multisampling is enabled, the multisample attachment will be used instead
// of the color attachment in opaque and transparent subpasses. It will get
// resolved in the last subpass that uses the depth attachment.
// Note that if only post-processing subpasses are used, the multisampling mode
// will be ignored even if the user specifies one.
struct SubpassConfig {
  // If true, the first subpass will use the color attachment and the depth
  // attachment. The depth attachment should be set to writable in the pipeline,
  // so that all opaque objects can be rendered in one subpass.
  bool use_opaque_subpass;

  // These subpasses will use the color attachment and the depth attachment, but
  // the depth attachment should not be writable. They are used for rendering
  // transparent objects.
  int num_transparent_subpasses;

  // These subpasses will only use the color attachment. The user may use them
  // to add effects like blurring, or add texts on the top of the frame.
  int num_post_processing_subpasses;
};

// If multisampling is not enabled, no attachment will be added internally at
// 'kMultisampleAttachmentIndex'. If only post-processing subpasses are used, no
// attachment will be added internally at either 'kDepthAttachmentIndex' or
// 'kMultisampleAttachmentIndex'.
enum AttachmentIndex {
  kColorAttachmentIndex = 0,
  kDepthAttachmentIndex,
  kMultisampleAttachmentIndex,
};

// If 'present_to_screen' is false, we assume that the color attachment will be
// read by other shaders.
std::unique_ptr<RenderPassBuilder> GetRenderPassBuilder(
    SharedBasicContext context, const SubpassConfig& subpass_config,
    int num_framebuffers, bool present_to_screen,
    absl::optional<MultisampleImage::Mode> multisampling_mode);

} /* namespace naive_render_pass */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H */
