//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct DrawCharInfo {
  alignas(16) glm::vec4 x_coords;
  alignas(16) glm::vec4 y_coords;
  alignas(16) glm::vec4 color_alpha;
};

} /* namespace */

StaticText::StaticText(SharedBasicContext context,
                       const std::vector<string>& texts,
                       Font font, int font_height)
    : Text{std::move(context)} {
  CharLoader loader{context, texts, font, font_height};
}

DynamicText::DynamicText(SharedBasicContext context,
                         const std::vector<string>& texts,
                         Font font, int font_height)
    : Text{std::move(context)} {
  CharLoader loader{context, texts, font, font_height};
}

void DynamicText::Draw(const string& text,
                       const glm::vec4& color_alpha,
                       const glm::vec2& coord,
                       AlignHorizontal align_horizontal,
                       AlignVertical align_vertical) const {

}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
