//
//  celestial.h
//
//  Created by Pujun Lun on 12/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_CELESTIAL_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_CELESTIAL_H

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
namespace aurora {

// This class wraps the rendering pipeline of an earth and a skybox.
// UpdateFramebuffer() must have been called before calling Draw() for the first
// time, and whenever the render pass is changed.
class Celestial {
 public:
  enum EarthTextureIndex {
    kEarthDayTextureIndex = 0,
    kEarthNightTextureIndex,
  };

  // When the frame is resized, the aspect ratio of viewport will always be
  // 'viewport_aspect_ratio'.
  Celestial(const wrapper::vulkan::SharedBasicContext& context,
            float viewport_aspect_ratio, int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Celestial(const Celestial&) = delete;
  Celestial& operator=(const Celestial&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  // For simplicity, the render area will be the same to 'frame_size'.
  void UpdateFramebuffer(
      const VkExtent2D& frame_size, VkSampleCountFlagBits sample_count,
      const wrapper::vulkan::RenderPass& render_pass, uint32_t subpass_index);

  // Updates per-frame data for earth.
  void UpdateEarthData(int frame, const common::Camera& camera,
                       const glm::mat4& model, EarthTextureIndex texture_index);

  // Updates per-frame data for skybox.
  void UpdateSkyboxData(int frame, const common::Camera& camera,
                        const glm::mat4& model);

  // Renders the earth and skybox.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, int frame) const;

 private:
  // Aspect ratio of the viewport. This is used to make sure the aspect ratio of
  // buttons does not change when the size of framebuffers changes.
  const float viewport_aspect_ratio_;

  // Objects used for rendering.
  std::unique_ptr<wrapper::vulkan::UniformBuffer> earth_uniform_;
  std::unique_ptr<wrapper::vulkan::PushConstant> earth_constant_;
  std::unique_ptr<wrapper::vulkan::PushConstant> skybox_constant_;
  std::unique_ptr<wrapper::vulkan::Model> earth_model_;
  std::unique_ptr<wrapper::vulkan::Model> skybox_model_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_EDITOR_CELESTIAL_H */
