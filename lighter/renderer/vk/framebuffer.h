//
//  framebuffer.h
//
//  Created by Pujun Lun on 10/2/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VK_FRAMEBUFFER_H
#define LIGHTER_RENDERER_VK_FRAMEBUFFER_H

#include <vector>

#include "lighter/renderer/ir/pass.h"
#include "lighter/renderer/vk/context.h"
#include "lighter/renderer/vk/util.h"

namespace lighter::renderer::vk {

class Framebuffers : public WithSharedContext {
 public:
  Framebuffers(const SharedContext& context, intl::RenderPass render_pass,
               const ir::RenderPassDescriptor& descriptor);
  
  // This class is neither copyable nor movable.
  Framebuffers(const Framebuffers&) = delete;
  Framebuffers& operator=(const Framebuffers&) = delete;

  ~Framebuffers() override;

 private:
  // Opaque image view objects.
  std::vector<intl::ImageView> image_views_;

  // Opaque framebuffer objects.
  std::vector<intl::Framebuffer> framebuffers_;
};

}  // namespace lighter::renderer::vk

#endif  // LIGHTER_RENDERER_VK_FRAMEBUFFER_H
