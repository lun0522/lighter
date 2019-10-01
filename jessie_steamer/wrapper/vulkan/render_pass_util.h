//
//  render_pass_util.h
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H

#include <memory>

#include "absl/types/optional.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

// This render pass contains one color attachment and one depth attachment.
// If 'multisampling_mode' has value, a multisample attachment will be contained
// as well, and configured to resolve to the color attachment.
// The render pass consists of subpasses of count 'num_subpasses'. Only the
// first subpass will use the depth attachment, since it is meant for rendering
// opaque objects. The user may add extra attachments and subpasses to render
// transparent objects and texts. Each subpass will wait for the previous
// subpass finish writing to the color attachment.
namespace naive_render_pass {

enum AttachmentIndex {
  kColorAttachmentIndex = 0,
  kDepthAttachmentIndex,
  kMultisampleAttachmentIndex,
};

enum SubpassIndex {
  kNativeSubpassIndex = 0,
  kFirstExtraSubpassIndex,
};

std::unique_ptr<RenderPassBuilder> GetNaiveRenderPassBuilder(
    SharedBasicContext context,
    int num_subpasses, int num_framebuffers, bool present_to_screen,
    absl::optional<MultisampleImage::Mode> multisampling_mode);

} /* namespace naive_render_pass */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H */
