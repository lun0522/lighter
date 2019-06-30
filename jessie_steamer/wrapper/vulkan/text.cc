//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include "absl/strings/str_format.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/util.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using common::VertexAttrib2D;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct DrawCharInfo {
  alignas(16) glm::vec4 x_coords;
  alignas(16) glm::vec4 y_coords;
  alignas(16) glm::vec4 color_alpha;
};

float GetVerticalOffset(float vertical_base, Text::Align align,
                        float total_width) {
  switch (align) {
    case Text::Align::kLeft:
      return vertical_base;
    case Text::Align::kCenter:
      return vertical_base - total_width / 2.0f;
    case Text::Align::kRight:
      return vertical_base - total_width;
  }
}

} /* namespace */

StaticText::StaticText(SharedBasicContext context,
                       const std::vector<std::string>& texts,
                       Font font, int font_height)
    : Text{std::move(context)} {
  CharLoader loader{context, texts, font, font_height};
}

void DynamicText::Draw(const std::string& text,
                       const glm::vec3& color, float alpha, float height,
                       float horizontal_base, float vertical_base,
                       Align align) {
  const float ratio = height / 1.0f;
  const auto& texture_map = char_loader_.char_texture_map();

  float total_width_in_tex_coord = 0.0f;
  for (auto c : text) {
    auto found = texture_map.find(c);
    if (found == texture_map.end()) {
      FATAL(absl::StrFormat("'%c' was not loaded", c));
    }
    total_width_in_tex_coord += found->second.advance;
  }
  const float initial_vertical_offset =
      GetVerticalOffset(vertical_base, align, total_width_in_tex_coord * ratio);

  std::vector<VertexAttrib2D> vertices;
  vertices.reserve(text_util::kNumVerticesPerChar * text.length());
  float vertical_offset = initial_vertical_offset;
  for (auto c : text) {
    const auto& texture_info = texture_map.find(c)->second;
    const glm::vec2& size_in_tex = texture_info.size;
    text_util::AppendCharPosAndTexCoord(
        /*pos_bottom_left=*/
        {vertical_offset + texture_info.bearing.x * ratio,
         horizontal_base + (texture_info.bearing.y - size_in_tex.y) * ratio},
        /*pos_increment=*/size_in_tex * ratio,
        /*tex_coord_bottom_left=*/{texture_info.offset_x, 0.0f},
        /*tex_coord_increment=*/size_in_tex,
        &vertices);
    vertical_offset += texture_info.advance * ratio;
  }
  const auto& indices = text_util::indices_per_char();

  vertex_buffer_.Init(PerVertexBuffer::InfoReuse{
      /*num_mesh=*/static_cast<int>(text.length()),
      /*per_mesh_vertices=*/
      {vertices, /*unit_count=*/text_util::kNumVerticesPerChar},
      /*shared_indices=*/{indices},
  });
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
