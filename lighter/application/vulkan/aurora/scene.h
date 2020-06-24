//
//  viewer.h
//
//  Created by Pujun Lun on 3/15/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_AURORA_SCENE_H
#define LIGHTER_APPLICATION_VULKAN_AURORA_SCENE_H

#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {

// This is the interface of scenes used in the aurora application.
class Scene {
 public:
  virtual ~Scene() = default;

  // Registers callbacks.
  virtual void OnEnter() = 0;

  // Unregisters callbacks.
  virtual void OnExit() = 0;

  // Recreates the swapchain and associated resources
  virtual void Recreate() = 0;

  // Updates per-frame data.
  virtual void UpdateData(int frame) = 0;

  // Renders the scene.
  // This should be called when 'command_buffer' is recording commands.
  virtual void Draw(const VkCommandBuffer& command_buffer,
                    uint32_t framebuffer_index, int current_frame) = 0;

  // Whether the application should transition to another scene.
  virtual bool ShouldTransitionScene() const = 0;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_AURORA_SCENE_H */
