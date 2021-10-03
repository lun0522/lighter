//
//  data.cc
//
//  Created by Pujun Lun on 10/2/21.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "lighter/common/data.h"

namespace lighter::common {

#define APPEND_ATTRIBUTES(attributes, type, member) \
    data::AppendVertexAttributes<decltype(type::member)>( \
        attributes, offsetof(type, member))

std::vector<VertexAttribute> Vertex2DPosOnly::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex2DPosOnly, pos);
  return attributes;
}

std::vector<VertexAttribute> Vertex2D::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex2D, pos);
  APPEND_ATTRIBUTES(attributes, Vertex2D, tex_coord);
  return attributes;
}

std::vector<VertexAttribute> Vertex3DPosOnly::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex3DPosOnly, pos);
  return attributes;
}

std::vector<VertexAttribute> Vertex3DWithColor::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex3DWithColor, pos);
  APPEND_ATTRIBUTES(attributes, Vertex3DWithColor, color);
  return attributes;
}

std::vector<VertexAttribute> Vertex3DWithTex::GetVertexAttributes() {
  std::vector<VertexAttribute> attributes;
  APPEND_ATTRIBUTES(attributes, Vertex3DWithTex, pos);
  APPEND_ATTRIBUTES(attributes, Vertex3DWithTex, norm);
  APPEND_ATTRIBUTES(attributes, Vertex3DWithTex, tex_coord);
  return attributes;
}

#undef APPEND_ATTRIBUTES

namespace data {

template <>
void AppendVertexAttributes<glm::mat4>(std::vector<VertexAttribute>& attributes,
                                       int offset_bytes) {
  attributes.reserve(attributes.size() + glm::mat4::length());
  for (int i = 0; i < glm::mat4::length(); ++i) {
    AppendVertexAttributes<glm::vec4>(attributes, offset_bytes);
    offset_bytes += sizeof(glm::vec4);
  }
}

}  // namespace data

std::array<Vertex2DPosOnly, 6> Vertex2DPosOnly::GetFullScreenSquadVertices() {
  return {
      Vertex2DPosOnly{.pos = {-1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = { 1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = { 1.0f,  1.0f}},
      Vertex2DPosOnly{.pos = {-1.0f, -1.0f}},
      Vertex2DPosOnly{.pos = { 1.0f,  1.0f}},
      Vertex2DPosOnly{.pos = {-1.0f,  1.0f}},
  };
}

std::array<Vertex2D, 6> Vertex2D::GetFullScreenSquadVertices(bool flip_y) {
  if (flip_y) {
    return {
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 1.0f}},
        Vertex2D{.pos = { 1.0f, -1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 1.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = {-1.0f,  1.0f}, .tex_coord = {0.0f, 0.0f}},
    };
  } else {
    return {
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 0.0f}},
        Vertex2D{.pos = { 1.0f, -1.0f}, .tex_coord = {1.0f, 0.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {-1.0f, -1.0f}, .tex_coord = {0.0f, 0.0f}},
        Vertex2D{.pos = { 1.0f,  1.0f}, .tex_coord = {1.0f, 1.0f}},
        Vertex2D{.pos = {-1.0f,  1.0f}, .tex_coord = {0.0f, 1.0f}},
    };
  }
}

}  // lighter::common
