//
//  distance_field.h
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_AURORA_VIEWER_DISTANCE_FIELD_H
#define LIGHTER_APPLICATION_VULKAN_AURORA_VIEWER_DISTANCE_FIELD_H

#include <memory>

#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used to generate distance field using jump flooding algorithm.
// Internally, it uses the output image as ping buffer to save device memory.
// The input image will not be modified.
class DistanceFieldGenerator {
 public:
  // 'input_image' and 'output_image' must have the same size. The generated
  // distance field will be written to 'output_image'.
  DistanceFieldGenerator(const renderer::vulkan::SharedBasicContext& context,
                         const renderer::vulkan::OffscreenImage& input_image,
                         const renderer::vulkan::OffscreenImage& output_image);

  // This class is neither copyable nor movable.
  DistanceFieldGenerator(const DistanceFieldGenerator&) = delete;
  DistanceFieldGenerator& operator=(const DistanceFieldGenerator&) = delete;

  // Generates the distance field. Note that before calling this, the user is
  // responsible for transitioning the layouts of 'input_image' so that it can
  // be linearly read in compute shaders, and the layouts of 'output_image' so
  // that it can be linearly read/write in compute shaders.
  // This should be called when 'command_buffer' is recording commands.
  void Generate(const VkCommandBuffer& command_buffer);

 private:
  // Directions when using ping-pong buffers.
  enum Direction {
    kInputToPing = 0,
    kPingToPong,
    kPongToPing,
    kPingToPing,
    kNumDirections,
  };

  // Invokes the compute shader.
  void Dispatch(const VkCommandBuffer& command_buffer,
                const renderer::vulkan::Pipeline& pipeline,
                Direction direction) const;

  // Number of work groups for invoking compute shaders.
  const VkExtent2D work_group_count_;

  // Step widths increase exponentially: 1, 2, 4, 8, ..., image dimension.
  int num_steps_ = 0;

  // Objects used for compute shaders.
  std::unique_ptr<renderer::vulkan::PushConstant> step_width_constant_;
  std::unique_ptr<renderer::vulkan::OffscreenImage> pong_image_;
  renderer::vulkan::Descriptor::ImageInfoMap image_info_maps_[kNumDirections];
  std::unique_ptr<renderer::vulkan::DynamicDescriptor> descriptor_;
  std::unique_ptr<renderer::vulkan::Pipeline> path_to_coord_pipeline_;
  std::unique_ptr<renderer::vulkan::Pipeline> jump_flooding_pipeline_;
  std::unique_ptr<renderer::vulkan::Pipeline> coord_to_dist_pipeline_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_AURORA_VIEWER_DISTANCE_FIELD_H */
