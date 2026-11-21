//
//  path_renderer.h
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_AURORA_VIEWER_PATH_RENDERER_H
#define LIGHTER_EXAMPLE_AURORA_VIEWER_PATH_RENDERER_H

#include <cstdint>
#include <glm/glm.hpp>
#include <span>

#include "lighter/common/camera.h"
#include "lighter/example/aurora/editor/path.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/type.h"

namespace lighter::example::aurora {

// Adds Frame Graph passes to rasterize 3D aurora splines with MSAA into an
// intermediate texture and thicken them in compute.
class PathRenderer2D {
 public:
  struct PassOutputs {
    renderer::ir::ImageHandle intermediate;
    renderer::ir::ImageHandle output;
  };

  PathRenderer2D(const glm::ivec2& image_extent,
                 renderer::ir::MultisamplingMode multisampling_mode);

  // This class is neither copyable nor movable.
  PathRenderer2D(const PathRenderer2D&) = delete;
  PathRenderer2D& operator=(const PathRenderer2D&) = delete;

  // Appends the graphics rasterization pass + compute thickening pass to
  // 'graph', returning the updated handles.
  PassOutputs AddPasses(renderer::ir::RenderGraph& graph,
                        renderer::ir::ImageHandle intermediate_image,
                        renderer::ir::ImageHandle output_image,
                        const common::Camera& camera,
                        std::span<const SplinePathView> spline_paths) const;

 private:
  const glm::ivec2 image_extent_;
  const renderer::ir::MultisamplingMode multisampling_mode_;
  const uint32_t group_count_x_;
  const uint32_t group_count_y_;

  renderer::ir::GraphicsPipelineDescriptor render_paths_pipeline_;
  renderer::ir::ComputePipelineDescriptor bold_paths_pipeline_;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_VIEWER_PATH_RENDERER_H
