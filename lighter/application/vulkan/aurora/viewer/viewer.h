//
//  viewer.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H
#define LIGHTER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H

#include <memory>
#include <vector>

#include "lighter/application/vulkan/aurora/scene.h"
#include "lighter/application/vulkan/aurora/viewer/path_dumper.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/render_pass.h"
#include "lighter/renderer/vulkan/wrapper/window_context.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace lighter {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used for rendering the aurora viewer scene using Vulkan APIs.
class ViewerRenderer {
 public:
  ViewerRenderer(const renderer::vulkan::WindowContext* window_context,
                 int num_frames_in_flight, float air_transmit_sample_step,
                 const renderer::vulkan::SamplableImage& aurora_paths_image,
                 const renderer::vulkan::SamplableImage& distance_field_image);

  // This class is neither copyable nor movable.
  ViewerRenderer(const ViewerRenderer&) = delete;
  ViewerRenderer& operator=(const ViewerRenderer&) = delete;

  // Updates internal states and rebuilds the graphics pipeline.
  void Recreate();

  // Updates camera parameters used to transform points to aurora paths texture.
  void UpdateDumpPathsCamera(const common::Camera& camera);

  // Updates camera parameters used for viewing aurora.
  void UpdateViewAuroraCamera(
      int frame, const common::PerspectiveCamera& camera);

  // Renders aurora paths.
  // This should be called when 'command_buffer' is recording commands.
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) const;

 private:
  // Objects used for rendering.
  const renderer::vulkan::WindowContext& window_context_;
  std::unique_ptr<renderer::vulkan::PushConstant> camera_constant_;
  std::unique_ptr<renderer::vulkan::UniformBuffer> render_info_uniform_;
  std::unique_ptr<renderer::vulkan::SharedTexture> aurora_deposition_image_;
  std::unique_ptr<renderer::vulkan::TextureImage> air_transmit_table_image_;
  std::unique_ptr<renderer::vulkan::SharedTexture> universe_skybox_image_;
  std::vector<std::unique_ptr<renderer::vulkan::StaticDescriptor>> descriptors_;
  std::unique_ptr<renderer::vulkan::PerVertexBuffer> vertex_buffer_;
  std::unique_ptr<renderer::vulkan::GraphicsPipelineBuilder> pipeline_builder_;
  std::unique_ptr<renderer::vulkan::Pipeline> pipeline_;
  std::unique_ptr<renderer::vulkan::RenderPassBuilder> render_pass_builder_;
  std::unique_ptr<renderer::vulkan::RenderPass> render_pass_;
};

// This class is used to manage and render the aurora viewer scene. The method
// of rendering aurora is modified from this paper:
// Lawlor, Orion & Genetti, Jon. (2011). Interactive Volume Rendering Aurora on
// the GPU. Journal of WSCG. 19. 25-32.
// Every time when aurora paths change, the user should call
// UpdateAuroraPaths(), and this class will perform following steps:
// (1) Render aurora path splines seen from the specified user viewpoint.
// (2) Bold those splines (note that we cannot specify line width on some
//     hardware when rendering those splines, so we need to add this pass).
// (3) Generate distance field.
// (4) Use ray tracing to render aurora paths.
class Viewer : public Scene {
 public:
  Viewer(renderer::vulkan::WindowContext* window_context,
         int num_frames_in_flight,
         std::vector<const renderer::vulkan::PerVertexBuffer*>&&
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
  void UpdateData(int frame) override {
    viewer_renderer_.UpdateViewAuroraCamera(
        frame, view_aurora_camera_->camera());
  }
  void Draw(const VkCommandBuffer& command_buffer,
            uint32_t framebuffer_index, int current_frame) override {
    viewer_renderer_.Draw(command_buffer, framebuffer_index, current_frame);
  }
  bool ShouldTransitionScene() const override { return should_quit_; }

 private:
  // Onscreen rendering context.
  renderer::vulkan::WindowContext& window_context_;

  // Whether we should quit this scene.
  bool should_quit_ = false;

  // Dumps aurora paths and generates distance field.
  PathDumper path_dumper_;

  // Renderer of aurora seen from the user viewpoint.
  ViewerRenderer viewer_renderer_;

  // Camera used for dumping aurora paths. We assume that:
  // (1) Aurora paths points are on a unit sphere.
  // (2) The camera is located at the center of sphere.
  // Hence, we would change the direction of this camera if the user viewpoint
  // changes. Other parameters keep unchanged.
  std::unique_ptr<common::Camera> dump_paths_camera_;

  // Camera used for viewing aurora. We would change both position and direction
  // of this camera when the user viewpoint changes, and when the user gives
  // inputs to change the direction.
  std::unique_ptr<common::UserControlledPerspectiveCamera> view_aurora_camera_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace lighter */

#endif /* LIGHTER_APPLICATION_VULKAN_AURORA_VIEWER_VIEWER_H */
