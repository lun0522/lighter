//
//  vertex_input_util.cc
//
//  Created by Pujun Lun on 6/19/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/vertex_input_util.h"

#include "jessie_steamer/common/file.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::VertexAttrib2D;
using common::VertexAttrib3D;
using std::vector;

} /* namespace */

vector<VkVertexInputBindingDescription> GetBindingDescriptions(
    const vector<VertexInputBinding>& bindings) {
  vector<VkVertexInputBindingDescription> descriptions;
  descriptions.reserve(bindings.size());
  for (const auto& binding : bindings) {
    descriptions.emplace_back(VkVertexInputBindingDescription{
        binding.binding_point,
        binding.data_size,
        binding.instancing ? VK_VERTEX_INPUT_RATE_INSTANCE :
        VK_VERTEX_INPUT_RATE_VERTEX,
    });
  }
  return descriptions;
}

template <>
VertexInputAttribute GetVertexAttributes<VertexAttrib2D>() {
  return VertexInputAttribute{
      kPerVertexBindingPoint,
      /*attributes=*/{
          VertexAttribute{
              /*location=*/0,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttrib2D, pos)),
              /*format=*/VK_FORMAT_R32G32_SFLOAT,
          },
          VertexAttribute{
              /*location=*/1,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttrib2D, tex_coord)),
              /*format=*/VK_FORMAT_R32G32_SFLOAT,
          },
      },
  };
}

template <>
VertexInputAttribute GetVertexAttributes<VertexAttrib3D>() {
  return VertexInputAttribute{
      kPerVertexBindingPoint,
      /*attributes=*/{
          VertexAttribute{
              /*location=*/0,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttrib3D, pos)),
              /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          },
          VertexAttribute{
              /*location=*/1,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttrib3D, norm)),
              /*format=*/VK_FORMAT_R32G32B32_SFLOAT,
          },
          VertexAttribute{
              /*location=*/2,
              /*offset=*/
              static_cast<uint32_t>(offsetof(VertexAttrib3D, tex_coord)),
              /*format=*/VK_FORMAT_R32G32_SFLOAT,
          },
      },
  };
}

vector<VkVertexInputAttributeDescription> GetAttributeDescriptions(
    const vector<VertexInputAttribute>& attributes) {
  int num_attribute = 0;
  for (const auto& attribs : attributes) {
    num_attribute += attribs.attributes.size();
  }

  vector<VkVertexInputAttributeDescription> descriptions;
  descriptions.reserve(num_attribute);
  for (const auto& attribs : attributes) {
    for (const auto& attrib : attribs.attributes) {
      descriptions.emplace_back(VkVertexInputAttributeDescription{
          attrib.location,
          attribs.binding_point,
          attrib.format,
          attrib.offset,
      });
    }
  }
  return descriptions;
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
