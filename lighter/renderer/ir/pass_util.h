//
//  pass_util.h
//
//  Created by Pujun Lun on 10/2/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_PASS_UTIL_H
#define LIGHTER_RENDERER_IR_PASS_UTIL_H

#include "lighter/renderer/ir/pass.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::renderer::ir::pass {

RenderPassDescriptor::ColorLoadStoreOps GetRenderTargetLoadStoreOps(
    bool is_multisampled) {
  return {
      AttachmentLoadOp::kClear,
      is_multisampled ? AttachmentStoreOp::kDontCare
                      : AttachmentStoreOp::kStore,
  };
}

RenderPassDescriptor::ColorLoadStoreOps GetResolveTargetLoadStoreOps() {
  return {AttachmentLoadOp::kDontCare, AttachmentStoreOp::kStore};
}

}  // lighter::renderer::ir::pass

#endif  // LIGHTER_RENDERER_IR_PASS_UTIL_H
