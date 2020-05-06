//
//  geometry_pass.h
//
//  Created by Pujun Lun on 5/5/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_GEOMETRY_PASS_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_GEOMETRY_PASS_H

#include <memory>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/model.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace troop {

// This class is used to handle the graphics pipeline for the geometry pass of
// deferred rendering.
class GeometryPass {
 public:
  enum ColorAttachmentIndex {
    kPositionImageIndex = 0,
    kNormalImageIndex,
    kDiffuseSpecularImageIndex,
    kNumColorAttachments,
  };

  GeometryPass(const wrapper::vulkan::SharedBasicContext& context,
               int num_frames_in_flight, float viewport_aspect_ratio,
               float model_scale, const glm::ivec2& num_soldiers,
               const glm::vec2& interval_between_soldiers);

  // This class is neither copyable nor movable.
  GeometryPass(const GeometryPass&) = delete;
  GeometryPass& operator=(const GeometryPass&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         const wrapper::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Updates per-frame data.
  void UpdatePerFrameData(int frame, const common::Camera& camera);

  // Runs the geometry pass.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, int frame) const;

 private:
  // Number of soldiers to render.
  const int num_soldiers_;

  // Objects used for rendering.
  std::unique_ptr<wrapper::vulkan::StaticPerInstanceBuffer> center_data_;
  std::unique_ptr<wrapper::vulkan::UniformBuffer> trans_uniform_;
  std::unique_ptr<wrapper::vulkan::Model> nanosuit_model_;
};

} /* namespace troop */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_TROOP_GEOMETRY_PASS_H */
