//
//  viewer.cc
//
//  Created by Pujun Lun on 11/3/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/application/vulkan/aurora/viewer/viewer.h"

#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace application {
namespace vulkan {
namespace aurora {
namespace {

using namespace wrapper::vulkan;

} /* namespace */

Viewer::Viewer(
    WindowContext* window_context, int num_frames_in_flight,
    std::vector<const PerVertexBuffer*>&& aurora_paths_vertex_buffers)
    : window_context_{*FATAL_IF_NULL(window_context)},
      path_dumper_{window_context_.basic_context(),
                   /*paths_image_dimension=*/2000,
                   /*camera_field_of_view=*/30.0f,
                   std::move(aurora_paths_vertex_buffers)} {

}

} /* namespace aurora */
} /* namespace vulkan */
} /* namespace application */
} /* namespace jessie_steamer */
