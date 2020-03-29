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

#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {

class PathDumper {
 public:
  PathDumper(const wrapper::vulkan::SharedBasicContext& context,
             int num_frames_in_flight, int paths_image_dimension,
             std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
                 aurora_paths_vertex_buffers);

  void DumpAuroraPaths(const glm::vec3& viewpoint_position);

 private:
  const std::vector<const wrapper::vulkan::PerVertexBuffer*>&&
      aurora_paths_vertex_buffers_;

  std::unique_ptr<wrapper::vulkan::OffscreenImage> paths_image_;
  std::unique_ptr<wrapper::vulkan::Image> multisample_image_;
  std::unique_ptr<wrapper::vulkan::PushConstant> trans_constant_;
  std::unique_ptr<wrapper::vulkan::RenderPass> render_pass_;
  std::unique_ptr<wrapper::vulkan::Pipeline> pipeline_;
};

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_APPLICATION_VULKAN_AURORA_VIEWER_PATH_DUMPER_H */
