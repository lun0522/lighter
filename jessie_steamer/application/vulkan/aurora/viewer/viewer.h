//
//  viewer.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H

#include "jessie_steamer/application/vulkan/aurora/scene.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class Viewer : public Scene {
 public:
  Viewer(wrapper::vulkan::WindowContext* window_context,
         int num_frames_in_flight) {}

  // This class is neither copyable nor movable.
  Viewer(const Viewer&) = delete;
  Viewer& operator=(const Viewer&) = delete;

  bool ShouldExit() const { return false; }

  // Overrides.
  void OnEnter() override {}
  void OnExit() override {}
  void Recreate() override {}
  void UpdateData(int frame) override {}
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) override {}
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H */
