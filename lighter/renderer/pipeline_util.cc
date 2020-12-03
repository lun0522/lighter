//
//  pipeline_util.cc
//
//  Created by Pujun Lun on 12/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/pipeline_util.h"

namespace lighter {
namespace renderer {
namespace pipeline {

GraphicsPipelineDescriptor::Viewport GetFullFrameViewport(
    const glm::ivec2& frame_size) {
  return {/*origin=*/glm::vec2{0.0f}, /*extent=*/frame_size};
}

GraphicsPipelineDescriptor::Viewport GetViewport(const glm::ivec2& frame_size,
                                                 float aspect_ratio) {
  glm::vec2 effective_size = frame_size;
  if (frame_size.x > frame_size.y * aspect_ratio) {
    effective_size.x = frame_size.y * aspect_ratio;
  } else {
    effective_size.y = frame_size.x / aspect_ratio;
  }
  return {/*origin=*/(glm::vec2{frame_size} - effective_size) / 2.0f,
          /*extent=*/effective_size};
}


} /* namespace pipeline */
} /* namespace renderer */
} /* namespace lighter */
