//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include "absl/strings/str_format.h"
#include "jessie_steamer/wrapper/vulkan/util.h"

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

void DynamicText::Draw(const std::string& text,
                       const glm::vec3& color, float alpha, float scale,
                       float horizontal_base, float vertical_base,
                       Align align) const {
  int total_width = 0;
  for (auto c : text) {
    auto found = char_loader_.char_texture_map().find(c);
    if (found == char_loader_.char_texture_map().end()) {
      FATAL(absl::StrFormat("'%c' was not loaded", c));
    }
    total_width += found->second.advance;
  }
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
