//
//  render_pass.h
//
//  Created by Pujun Lun on 1/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDER_PASS_H
#define LIGHTER_RENDERER_VK_RENDER_PASS_H

#include <vector>

#include "lighter/renderer/ir/pass.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/framebuffer.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

class RenderPass : public WithSharedContext,
                   public ir::RenderPass {
 public:
  RenderPass(const SharedContext& context,
             const ir::RenderPassDescriptor& descriptor);

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass() override;

 private:
  // Opaque render pass object.
  intl::RenderPass render_pass_;

  // Manages framebuffers and associated image views.
  const Framebuffers framebuffers_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_RENDER_PASS_H
