//
//  path_renderer.h
//
//  Created by Pujun Lun on 4/18/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_RENDERER_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_RENDERER_H

#include <memory>
#include <vector>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used to dump aurora paths to an image, and bold them. When
// aurora paths change, the user should call RenderPaths() and BoldPaths() to
// re-render them.
class PathRenderer2D {
 public:
  // The user should provide 'intermediate_image' that has the same size as
  // 'output_image', so that we can use it to bold rendered aurora paths.
  PathRenderer2D(const wrapper::vulkan::SharedBasicContext& context,
                 const wrapper::vulkan::OffscreenImage& intermediate_image,
                 const wrapper::vulkan::OffscreenImage& output_image,
                 wrapper::vulkan::MultisampleImage::Mode multisampling_mode,
                 std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                     aurora_paths_vertex_buffers);

  // This class is neither copyable nor movable.
  PathRenderer2D(const PathRenderer2D&) = delete;
  PathRenderer2D& operator=(const PathRenderer2D&) = delete;

  // Renders aurora paths.
  // This should be called when 'command_buffer' is recording commands.
  void RenderPaths(const VkCommandBuffer& command_buffer,
                   const common::Camera& camera);

  // Bolds rendered aurora paths. Note that before calling this, the user is
  // responsible for transitioning the layouts of 'intermediate_image' and
  // 'output_image' so that they can be linearly accessed in compute shaders.
  // This should be called when 'command_buffer' is recording commands.
  void BoldPaths(const VkCommandBuffer& command_buffer);

 private:
  // Number of work groups for invoking compute shaders.
  const VkExtent2D work_group_count_;

  // Objects used for graphics and compute pipelines.
  const std::vector<const wrapper::vulkan::PerVertexBuffer*>
      aurora_paths_vertex_buffers_;
  std::unique_ptr<wrapper::vulkan::Image> multisample_image_;
  std::unique_ptr<wrapper::vulkan::PushConstant> trans_constant_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
  wrapper::vulkan::RenderPass::RenderOp render_op_;
  std::unique_ptr<wrapper::vulkan::StaticDescriptor> bold_paths_descriptor_;
  std::unique_ptr<wrapper::vulkan::Pipeline> render_paths_pipeline_;
  std::unique_ptr<wrapper::vulkan::Pipeline> bold_paths_pipeline_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_RENDERER_H */
