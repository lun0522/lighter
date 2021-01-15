//
//  render_pass.h
//
//  Created by Pujun Lun on 1/14/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_RENDER_PASS_H
#define LIGHTER_RENDERER_VK_RENDER_PASS_H

#include <vector>

#include "lighter/renderer/pass.h"
#include "lighter/renderer/vk/pipeline.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter::renderer::vk {

class RenderPass : public renderer::RenderPass {
 public:
  RenderPass(SharedContext context, const RenderPassDescriptor& descriptor);

  // This class is neither copyable nor movable.
  RenderPass(const RenderPass&) = delete;
  RenderPass& operator=(const RenderPass&) = delete;

  ~RenderPass() override;

 private:
  // Pointer to context.
  const SharedContext context_;

  // Graphics pipelines used in this render pass.
  std::vector<Pipeline> pipelines_;

  // Opaque render pass object.
  VkRenderPass render_pass_;

  // Opaque framebuffer objects.
  std::vector<VkFramebuffer> framebuffers_;
};

}  // namespace vk::renderer::lighter

#endif  // LIGHTER_RENDERER_VK_RENDER_PASS_H
