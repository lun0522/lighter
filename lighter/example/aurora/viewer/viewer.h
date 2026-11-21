//
//  viewer.h
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_EXAMPLE_AURORA_VIEWER_VIEWER_H
#define LIGHTER_EXAMPLE_AURORA_VIEWER_VIEWER_H

#include <glm/glm.hpp>
#include <memory>
#include <span>
#include <vector>

#include "lighter/base/window.h"
#include "lighter/example/aurora/editor/path.h"
#include "lighter/example/aurora/viewer/path_dumper.h"
#include "lighter/renderer/ir/buffer.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/pipeline.h"
#include "lighter/renderer/ir/render_graph.h"
#include "lighter/renderer/ir/renderer.h"
#include "lighter/renderer/ir/type.h"
#include "lighter/scene/camera.h"
#include "lighter/scene/scene.h"

namespace lighter::example::aurora {

// Volumetric aurora ray-tracing viewer scene.
class Viewer : public scene::Scene {
 public:
  Viewer(base::Window& window, renderer::ir::Renderer& renderer,
         int window_index, renderer::ir::MultisamplingMode multisampling_mode,
         int num_frames_in_flight);

  // This class is neither copyable nor movable.
  Viewer(const Viewer&) = delete;
  Viewer& operator=(const Viewer&) = delete;

  // Re-dumps aurora paths & distance field from 'viewpoint_position'.
  void UpdateAuroraPaths(const glm::vec3& viewpoint_position,
                         std::span<const SplinePathView> spline_paths);

  void OnEnter() override;
  void OnExit() override;
  void OnSurfaceRecreated() override;
  void BuildFrame(renderer::ir::RenderGraph& graph, int frame_slot) override;
  bool ShouldTransitionScene() const override { return should_quit_; }

 private:
  base::Window& window_;
  renderer::ir::Renderer& renderer_;
  const int window_index_;
  const renderer::ir::MultisamplingMode multisampling_mode_;
  bool should_quit_ = false;

  PathDumper path_dumper_;

  std::unique_ptr<renderer::ir::Buffer> quad_vertex_buffer_;
  std::vector<std::unique_ptr<renderer::ir::Buffer>> render_info_uniforms_;
  std::unique_ptr<renderer::ir::Image> aurora_deposition_image_;
  std::unique_ptr<renderer::ir::Image> air_transmit_table_image_;
  std::unique_ptr<renderer::ir::Image> universe_skybox_image_;
  renderer::ir::SamplerDescriptor clamp_sampler_{};
  renderer::ir::SamplerDescriptor repeat_sampler_{};
  renderer::ir::GraphicsPipelineDescriptor base_pipeline_descriptor_;

  glm::mat4 aurora_proj_view_{1.0f};
  std::unique_ptr<scene::PerspectiveCamera> dump_paths_camera_;
  std::unique_ptr<scene::UserControlledPerspectiveCamera> view_aurora_camera_;
};

}  // namespace lighter::example::aurora

#endif  // LIGHTER_EXAMPLE_AURORA_VIEWER_VIEWER_H
