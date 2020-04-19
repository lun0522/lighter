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

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class ViewerRenderer {
 public:
  ViewerRenderer(const wrapper::vulkan::SharedBasicContext& context,
                 int num_frames_in_flight,
                 const wrapper::vulkan::SamplableImage& aurora_paths_image,
                 const wrapper::vulkan::SamplableImage& distance_field_image);

  // This class is neither copyable nor movable.
  ViewerRenderer(const ViewerRenderer&) = delete;
  ViewerRenderer& operator=(const ViewerRenderer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void UpdateFramebuffer(const VkExtent2D& frame_size,
                         const wrapper::vulkan::RenderPass& render_pass,
                         uint32_t subpass_index);

  // Updates per-frame data.
  void UpdateData(int frame, const common::Camera& camera);

  // Renders aurora paths.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer, int frame) const;

 private:
  std::unique_ptr<wrapper::vulkan::UniformBuffer> camera_uniform_;
  std::unique_ptr<wrapper::vulkan::SharedTexture> aurora_deposition_image_;
  std::vector<std::unique_ptr<wrapper::vulkan::StaticDescriptor>> descriptors_;
  std::unique_ptr<wrapper::vulkan::PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<wrapper::vulkan::GraphicsPipelineBuilder> pipeline_builder_;
  std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
};

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
  void UpdateAuroraPaths(const glm::vec3& viewpoint_position);

  // Overrides.
  void OnEnter() override;
  void OnExit() override;
  void Recreate() override;
  void UpdateData(int frame) override;
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) override;
  bool ShouldTransitionScene() const override { return did_press_right_; }

 private:
  // On-screen rendering context.
  wrapper::vulkan::WindowContext& window_context_;

  bool did_press_right_ = false;
  PathDumper path_dumper_;
  ViewerRenderer viewer_renderer_;
  std::unique_ptr<common::Camera> camera_;
  std::unique_ptr<wrapper::vulkan::NaiveRenderPassBuilder> render_pass_builder_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H */
