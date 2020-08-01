//
//  naive_render_pass.h
//
//  Created by Pujun Lun on 7/31/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H

#include <memory>

#include "lighter/renderer/vulkan/extension/attachment_info.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "third_party/absl/types/optional.h"

namespace lighter {
namespace renderer {
namespace vulkan {

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

    int num_subpasses() const {
      return num_opaque_subpass_ + num_transparent_subpasses_ +
             num_overlay_subpasses_;
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

  static std::unique_ptr<RenderPassBuilder> CreateBuilder(
      SharedBasicContext context, int num_framebuffers,
      const SubpassConfig& subpass_config);
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_NAIVE_RENDER_PASS_H */
