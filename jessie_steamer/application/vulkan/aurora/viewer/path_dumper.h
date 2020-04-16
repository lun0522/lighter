//
//  path_dumper.h
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H

#include <array>
#include <memory>
#include <vector>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/image_util.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class PathDumper {
 public:
  PathDumper(wrapper::vulkan::SharedBasicContext context,
             int paths_image_dimension, float camera_field_of_view,
             std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                 aurora_paths_vertex_buffers);

  // This class is neither copyable nor movable.
  PathDumper(const PathDumper&) = delete;
  PathDumper& operator=(const PathDumper&) = delete;

  void DumpAuroraPaths(const glm::vec3& viewpoint_position);

  const wrapper::vulkan::SamplableImage& paths_image() const {
    return *paths_images_[1];
  }

 private:
  class PathRenderer {
   public:
    PathRenderer(const wrapper::vulkan::SharedBasicContext& context,
                 const wrapper::vulkan::Image& paths_image,
                 std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                     aurora_paths_vertex_buffers);

    // This class is neither copyable nor movable.
    PathRenderer(const PathRenderer&) = delete;
    PathRenderer& operator=(const PathRenderer&) = delete;

    void UpdateData(const common::Camera& camera);

    void Draw(const VkCommandBuffer& command_buffer);

   private:
    const std::vector<const wrapper::vulkan::PerVertexBuffer*>
        aurora_paths_vertex_buffers_;
    std::unique_ptr<wrapper::vulkan::Image> multisample_image_;
    std::unique_ptr<wrapper::vulkan::PushConstant> trans_constant_;
    std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
    wrapper::vulkan::RenderPass::RenderOp render_op_;
    std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
  };

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  std::unique_ptr<common::Camera> camera_;
  std::array<std::unique_ptr<wrapper::vulkan::OffscreenImage>, 2> paths_images_;
  std::unique_ptr<wrapper::vulkan::ImageLayoutManager> image_layout_manager_;
  std::unique_ptr<PathRenderer> path_renderer_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H */
