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

#include "jessie_steamer/application/vulkan/aurora/viewer/distance_field.h"
#include "jessie_steamer/application/vulkan/aurora/viewer/path_renderer.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
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

  // Accessors.
  const wrapper::vulkan::SamplableImage& paths_image() const {
    return *paths_image_;
  }
  const wrapper::vulkan::SamplableImage& distance_field_image() const {
    return *distance_field_image_;
  }

 private:
  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  std::unique_ptr<common::Camera> camera_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> paths_image_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> distance_field_image_;
  std::unique_ptr<wrapper::vulkan::image::LayoutManager> image_layout_manager_;
  std::unique_ptr<PathRenderer2D> path_renderer_;
  std::unique_ptr<DistanceFieldGenerator> distance_field_generator_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H */
