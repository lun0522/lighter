//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "model.h"

#include <vector>

#include "context.h"
#include "util.h"

namespace wrapper {
namespace vulkan {
namespace {

using std::array;
using util::VertexAttrib;

} /* namespace */

void Model::Init(SharedContext context,
                 const std::string& path,
                 int index_base) {
  std::vector<VertexAttrib> vertices;
  std::vector<uint32_t> indices;
  util::LoadObjFile("texture/cube.obj", 1, &vertices, &indices);

  buffer::DataInfo vertex_info{
      vertices.data(),
      sizeof(vertices[0]) * vertices.size(),
      CONTAINER_SIZE(vertices),
  };
  buffer::DataInfo index_info{
      indices.data(),
      sizeof(indices[0]) * indices.size(),
      CONTAINER_SIZE(indices),
  };
  vertex_buffer_.Init(context, vertex_info, index_info);
}

std::vector<VkVertexInputBindingDescription> Model::binding_descs() {
  return {
      VkVertexInputBindingDescription{
          /*binding=*/0,
          /*stride=*/sizeof(VertexAttrib),
          // for instancing, use _INSTANCE for .inputRate
          /*inputRate=*/VK_VERTEX_INPUT_RATE_VERTEX,
      },
  };
}

std::vector<VkVertexInputAttributeDescription> Model::attrib_descs() {
  return {
      VkVertexInputAttributeDescription{
          /*location=*/0, // layout (location = 0) in
          /*binding=*/0, // which binding point does data come from
          /*format=*/VK_FORMAT_R32G32B32_SFLOAT, // implies total size
          /*offset=*/offsetof(VertexAttrib, pos), // reading offset
      },
      VkVertexInputAttributeDescription{
          /*location=*/1,
          /*binding=*/0,
          /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          /*offset=*/offsetof(VertexAttrib, norm),
      },
      VkVertexInputAttributeDescription{
          /*location=*/2,
          /*binding=*/0,
          /*format=*/VK_FORMAT_R32G32_SFLOAT,
          /*offset=*/offsetof(VertexAttrib, tex_coord),
      },
  };
}

} /* namespace vulkan */
} /* namespace wrapper */
