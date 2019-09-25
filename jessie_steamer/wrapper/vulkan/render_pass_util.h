//
//  render_pass_util.h
//
//  Created by Pujun Lun on 9/24/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H

#include "absl/types/optional.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

namespace naive_render_pass {

// Contains one color attachment (at index 0) and one depth attachment (at
// index 1). Only the first subpass will use the depth attachment and is
// intended for rendering opaque objects. Following subpasses are intended for
// transparent objects and text. Each of them will depend on the previous one.
// Only set_framebuffer_size() and update_image() need to be called when the
// window is resized.
enum AttachmentIndex {
  kColorAttachmentIndex = 0,
  kDepthStencilAttachmentIndex,
  kMultisampleAttachmentIndex,
};

std::unique_ptr<RenderPassBuilder> NaiveRenderPassBuilder(
    SharedBasicContext context,
    int num_subpass, int num_swapchain_image,
    absl::optional<MultisampleImage::Mode> multisampling_mode);

} /* namespace naive_render_pass */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_RENDER_PASS_UTIL_H */
