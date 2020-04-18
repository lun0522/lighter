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
  // Note that 'paths_image_dimension' must be power of 2.
  PathDumper(wrapper::vulkan::SharedBasicContext context,
             int paths_image_dimension, float camera_field_of_view,
             std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                 aurora_paths_vertex_buffers);

  // This class is neither copyable nor movable.
  PathDumper(const PathDumper&) = delete;
  PathDumper& operator=(const PathDumper&) = delete;

  void DumpAuroraPaths(const glm::vec3& viewpoint_position);

  const wrapper::vulkan::SamplableImage& paths_image() const {
    return *images_[kPingImageIndex];
  }
  const wrapper::vulkan::SamplableImage& distance_field() const {
    return *images_[kPongImageIndex];
  }

 private:
  enum ImageIndex { kPingImageIndex = 0, kPongImageIndex, kNumImages };

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

  class DistanceFieldGenerator {
   public:
    DistanceFieldGenerator(
        const wrapper::vulkan::SharedBasicContext& context,
        const std::array<std::unique_ptr<wrapper::vulkan::OffscreenImage>,
                         kNumImages>& images,
        const wrapper::vulkan::image::LayoutManager& image_layout_manager);

    // This class is neither copyable nor movable.
    DistanceFieldGenerator(const DistanceFieldGenerator&) = delete;
    DistanceFieldGenerator& operator=(const DistanceFieldGenerator&) = delete;

    void Generate(const VkCommandBuffer& command_buffer);

   private:
    enum Direction {kPingToPong = 0, kPongToPing, kPongToPong, kNumDirections };

    void UpdateDescriptor(const VkCommandBuffer& command_buffer,
                          const wrapper::vulkan::Pipeline& pipeline,
                          Direction direction) const;

    int num_steps_ = 0;
    std::unique_ptr<wrapper::vulkan::PushConstant> step_width_constant_;
    wrapper::vulkan::Descriptor::ImageInfoMap image_info_maps_[kNumDirections];
    std::unique_ptr<wrapper::vulkan::DynamicDescriptor> descriptor_;
    VkExtent2D work_group_size_{};
    std::unique_ptr<wrapper::vulkan::Pipeline> path_to_coord_pipeline_;
    std::unique_ptr<wrapper::vulkan::Pipeline> jump_flooding_pipeline_;
    std::unique_ptr<wrapper::vulkan::Pipeline> coord_to_distance_pipeline_;
  };

  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  std::unique_ptr<common::Camera> camera_;
  std::array<std::unique_ptr<wrapper::vulkan::OffscreenImage>, kNumImages>
      images_;
  std::unique_ptr<wrapper::vulkan::image::LayoutManager> image_layout_manager_;
  std::unique_ptr<PathRenderer> path_renderer_;
  std::unique_ptr<DistanceFieldGenerator> distance_field_generator_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H */
