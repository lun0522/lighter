//
//  path_dumper.h
//
//  Created by Pujun Lun on 3/28/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H
#define JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H

#include <memory>
#include <vector>

#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
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

  void DumpAuroraPaths(const glm::vec3& viewpoint_position);

  const wrapper::vulkan::SamplableImage& paths_image() const {
    return bold_paths_pass_->bold_paths_image();
  }

 private:
  class DumpPathsPass {
   public:
    DumpPathsPass(const wrapper::vulkan::SharedBasicContext& context,
                  const VkExtent2D& image_extent,
                  std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                      aurora_paths_vertex_buffers);

    void UpdateData(const common::Camera& camera);

    void Draw(const VkCommandBuffer& command_buffer);

    const wrapper::vulkan::SamplableImage& paths_image() const {
      return *paths_image_;
    }

   private:
    const std::vector<const wrapper::vulkan::PerVertexBuffer*>
        aurora_paths_vertex_buffers_;
    std::unique_ptr<wrapper::vulkan::OffscreenImage> paths_image_;
    std::unique_ptr<wrapper::vulkan::Image> multisample_image_;
    std::unique_ptr<wrapper::vulkan::PushConstant> trans_constant_;
    std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
    std::vector<wrapper::vulkan::RenderPass::RenderOp> render_ops_;
    std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
  };

  class BoldPathsPass {
   public:
    BoldPathsPass(const wrapper::vulkan::SharedBasicContext& context,
                  const wrapper::vulkan::SamplableImage& paths_image,
                  const VkExtent2D& image_extent);

    void Draw(const VkCommandBuffer& command_buffer);

    const wrapper::vulkan::SamplableImage& bold_paths_image() const {
      return *bold_paths_image_;
    }

   private:
    std::unique_ptr<wrapper::vulkan::StaticDescriptor> descriptor_;
    std::unique_ptr<wrapper::vulkan::PerVertexBuffer> vertex_buffer_;
    std::unique_ptr<wrapper::vulkan::OffscreenImage> bold_paths_image_;
    std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
    std::vector<wrapper::vulkan::RenderPass::RenderOp> render_ops_;
    std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
  };

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  std::unique_ptr<common::Camera> camera_;
  std::unique_ptr<DumpPathsPass> dump_paths_pass_;
  std::unique_ptr<BoldPathsPass> bold_paths_pass_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H */