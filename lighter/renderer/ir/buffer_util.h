//
//  buffer_util.h
//
//  Created by Pujun Lun on 12/1/20.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_IR_BUFFER_UTIL_H
#define LIGHTER_RENDERER_IR_BUFFER_UTIL_H

#include <vector>

#include "lighter/renderer/ir/buffer.h"

namespace lighter::renderer::ir::buffer {

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex2DPosOnly(
    int loc_pos);

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex2D(
    int loc_pos, int loc_tex_coord);

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex3DPosOnly(
    int loc_pos);

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex3DWithColor(
    int loc_pos, int loc_color);

std::vector<VertexBufferView::Attribute> CreateAttributesForVertex3DWithTex(
    int loc_pos, int loc_norm, int loc_tex_coord);

}  // namespace lighter::renderer::ir::buffer

#endif  // LIGHTER_RENDERER_IR_BUFFER_UTIL_H
