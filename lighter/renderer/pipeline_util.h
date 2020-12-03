//
//  pipeline_util.h
//
//  Created by Pujun Lun on 12/2/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_PIPELINE_UTIL_H
#define LIGHTER_RENDERER_PIPELINE_UTIL_H

#include "lighter/renderer/pipeline.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {
namespace pipeline {

// Returns a viewport transform targeting the full frame of 'frame_size'.
GraphicsPipelineDescriptor::Viewport GetFullFrameViewport(
    const glm::ivec2& frame_size);

// Returns a viewport transform that keeps the aspect ratio of objects
// unchanged, and fills the frame as much as possible.
GraphicsPipelineDescriptor::Viewport GetViewport(const glm::ivec2& frame_size,
                                                 float aspect_ratio);

} /* namespace pipeline */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_PIPELINE_UTIL_H */
