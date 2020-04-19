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

#include "jessie_steamer/application/vulkan/aurora/viewer/distance_field.h"
#include "jessie_steamer/application/vulkan/aurora/viewer/path_renderer.h"
#include "jessie_steamer/common/camera.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/image_util.h"
#include "third_party/glm/glm.hpp"
#include "third_party/vulkan/vulkan.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

// This class is used to dump aurora paths and generate distance field.
class PathDumper {
 public:
  // Note that 'paths_image_dimension' must be power of 2.
  PathDumper(wrapper::vulkan::SharedBasicContext context,
             int paths_image_dimension,
             std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                 aurora_paths_vertex_buffers);

  // This class is neither copyable nor movable.
  PathDumper(const PathDumper&) = delete;
  PathDumper& operator=(const PathDumper&) = delete;

  // Dumps aurora paths and generates distance field. We only care about aurora
  // paths that are visible from the view of 'camera'.
  void DumpAuroraPaths(const common::Camera& camera);

  // Accessors.
  const wrapper::vulkan::SamplableImage& aurora_paths_image() const {
    return *paths_image_;
  }
  const wrapper::vulkan::SamplableImage& distance_field_image() const {
    return *distance_field_image_;
  }

 private:
  // Pointer to context.
  const wrapper::vulkan::SharedBasicContext context_;

  // Generated images.
  std::unique_ptr<wrapper::vulkan::OffscreenImage> paths_image_;
  std::unique_ptr<wrapper::vulkan::OffscreenImage> distance_field_image_;

  // Manages layouts of images.
  std::unique_ptr<wrapper::vulkan::image::LayoutManager> image_layout_manager_;

  // Dumps and bolds aurora paths.
  std::unique_ptr<PathRenderer2D> path_renderer_;

  // Generates distance field.
  std::unique_ptr<DistanceFieldGenerator> distance_field_generator_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H */
