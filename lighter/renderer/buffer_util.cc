//
//  buffer_util.cc
//
//  Created by Pujun Lun on 12/1/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/renderer/buffer_util.h"

#include "lighter/common/file.h"
#include "lighter/renderer/type.h"

namespace lighter::renderer::buffer {
namespace {

template <typename T>
DataFormat GetDataFormat();

template <>
DataFormat GetDataFormat<glm::vec2>() { return DataFormat::kSFloat32Vec2; }

template <>
DataFormat GetDataFormat<glm::vec3>() { return DataFormat::kSFloat32Vec3; }

}  // namespace

#define CREATE_ATTRIBUTE(type, member, location) \
    {location, GetDataFormat<decltype(type::member)>(), offsetof(type, member)}

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex2DPosOnly(
    int loc_pos) {
  return {CREATE_ATTRIBUTE(common::Vertex2DPosOnly, pos, loc_pos)};
}

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex2D(
    int loc_pos, int loc_tex_coord) {
  return {
      CREATE_ATTRIBUTE(common::Vertex2D, pos, loc_pos),
      CREATE_ATTRIBUTE(common::Vertex2D, tex_coord, loc_tex_coord),
  };
}

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex3DPosOnly(
    int loc_pos) {
  return {CREATE_ATTRIBUTE(common::Vertex3DPosOnly, pos, loc_pos)};
}

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex3DWithColor(
    int loc_pos, int loc_color) {
  return {
      CREATE_ATTRIBUTE(common::Vertex3DWithColor, pos, loc_pos),
      CREATE_ATTRIBUTE(common::Vertex3DWithColor, color, loc_color),
  };
}

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex3DWithTex(
    int loc_pos, int loc_norm, int loc_tex_coord) {
  return {
      CREATE_ATTRIBUTE(common::Vertex3DWithTex, pos, loc_pos),
      CREATE_ATTRIBUTE(common::Vertex3DWithTex, norm, loc_norm),
      CREATE_ATTRIBUTE(common::Vertex3DWithTex, tex_coord, loc_tex_coord),
  };
}

#undef CREATE_ATTRIBUTE

}  // namespace lighter::renderer::buffer
