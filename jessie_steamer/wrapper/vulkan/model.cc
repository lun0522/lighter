//
//  model.cc
//
//  Created by Pujun Lun on 3/22/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/model.h"

#include <vector>

#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/context.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::util::VertexAttrib3D;
using std::vector;

} /* namespace */

void Model::Init(SharedContext context,
                 const std::string& path,
                 int index_base) {
  vector<VertexAttrib3D> vertices;
  vector<uint32_t> indices;
  common::util::LoadObjFromFile(path, index_base, &vertices, &indices);

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
  vertex_buffer_.Init(std::move(context), vertex_info, index_info);
}

const vector<VkVertexInputBindingDescription>& Model::binding_descs() {
  static const vector<VkVertexInputBindingDescription> descriptions{
      VkVertexInputBindingDescription{
          /*binding=*/0,
          /*stride=*/sizeof(VertexAttrib3D),
          // for instancing, use _INSTANCE for .inputRate
          /*inputRate=*/VK_VERTEX_INPUT_RATE_VERTEX,
      },
  };
  return descriptions;
}

const vector<VkVertexInputAttributeDescription>& Model::attrib_descs() {
  static const vector<VkVertexInputAttributeDescription> descriptions{
      VkVertexInputAttributeDescription{
          /*location=*/0,  // layout (location = 0) in
          /*binding=*/0,  // which binding point does data come from
          /*format=*/VK_FORMAT_R32G32B32_SFLOAT,  // implies total size
          /*offset=*/offsetof(VertexAttrib3D, pos),  // reading offset
      },
      VkVertexInputAttributeDescription{
          /*location=*/1,
          /*binding=*/0,
          /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          /*offset=*/offsetof(VertexAttrib3D, norm),
      },
      VkVertexInputAttributeDescription{
          /*location=*/2,
          /*binding=*/0,
          /*format=*/VK_FORMAT_R32G32_SFLOAT,
          /*offset=*/offsetof(VertexAttrib3D, tex_coord),
      },
  };
  return descriptions;
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
