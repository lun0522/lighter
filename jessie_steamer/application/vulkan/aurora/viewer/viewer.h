//
//  viewer.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H

#include <vector>

#include "jessie_steamer/application/vulkan/aurora/scene.h"
#include "jessie_steamer/application/vulkan/aurora/viewer/path_dumper.h"
#include "jessie_steamer/application/vulkan/util.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/window_context.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

// TODO: Separate renderer class.
class Viewer : public Scene {
 public:
  Viewer(wrapper::vulkan::WindowContext* window_context,
         int num_frames_in_flight,
         std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
             aurora_paths_vertex_buffers);

  // This class is neither copyable nor movable.
  Viewer(const Viewer&) = delete;
  Viewer& operator=(const Viewer&) = delete;

  // Dumps aurora paths viewed from 'viewpoint_position'.
  void UpdateAuroraPaths(const glm::vec3& viewpoint_position) {
    path_dumper_.DumpAuroraPaths(viewpoint_position);
  }

  // Overrides.
  void OnEnter() override;
  void OnExit() override;
  void Recreate() override;
  void UpdateData(int frame) override {}
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) override;
  bool ShouldTransitionScene() const override { return did_press_right_; }

 private:
  // On-screen rendering context.
  wrapper::vulkan::WindowContext& window_context_;

  bool did_press_right_ = false;
  PathDumper path_dumper_;
  std::unique_ptr<ImageViewer> image_viewer_;
  std::unique_ptr<wrapper::vulkan::PerFrameCommand> command_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H */
