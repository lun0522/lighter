//
//  distance_field.h
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_SCENE_DISTANCE_FIELD_H
#define LIGHTER_SCENE_DISTANCE_FIELD_H

#include <cstdint>
#include <glm/glm.hpp>
#include <vector>

#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"

namespace lighter::scene {

// Adds Frame Graph compute passes implementing the Jump Flooding Algorithm
// (JFA) to convert a binary mask texture into a 2D Euclidean distance field.
class DistanceFieldGenerator {
 public:
  explicit DistanceFieldGenerator(const glm::ivec2& image_extent);

  // This class is neither copyable nor movable.
  DistanceFieldGenerator(const DistanceFieldGenerator&) = delete;
  DistanceFieldGenerator& operator=(const DistanceFieldGenerator&) = delete;

  // Appends JFA compute passes to 'graph' reading 'input_image' and writing
  // the distance field into 'ping_image'. Returns updated handles.
  renderer::ir::ImageHandle AddPasses(
      renderer::ir::RenderGraph& graph, renderer::ir::ImageHandle input_image,
      renderer::ir::ImageHandle ping_image) const;

 private:
  const glm::ivec2 image_extent_;
  const uint32_t group_count_x_;
  const uint32_t group_count_y_;
  std::vector<int> step_widths_;

  renderer::ir::ComputePipelineDescriptor path_to_coord_pipeline_;
  renderer::ir::ComputePipelineDescriptor jump_flooding_pipeline_;
  renderer::ir::ComputePipelineDescriptor coord_to_dist_pipeline_;
};

}  // namespace lighter::scene

#endif  // LIGHTER_SCENE_DISTANCE_FIELD_H
