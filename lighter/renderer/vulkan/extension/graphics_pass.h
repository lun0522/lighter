//
//  graphics_pass.h
//
//  Created by Pujun Lun on 6/27/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H
#define LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H

#include "lighter/renderer/vulkan/extension/base_pass.h"

namespace lighter {
namespace renderer {
namespace vulkan {

class GraphicsPass : public BasePass {
 public:
  // Inherits constructor.
  using BasePass::BasePass;

  // This class is neither copyable nor movable.
  GraphicsPass(const GraphicsPass&) = delete;
  GraphicsPass& operator=(const GraphicsPass&) = delete;
};

} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_EXTENSION_GRAPHICS_PASS_H */
