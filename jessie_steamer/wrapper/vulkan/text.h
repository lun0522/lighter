//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

enum class Font { kGeorgia, kOstrich };
enum class AlignHorizontal { kLeft, kCenter, kRight };
enum class AlignVertical { kTop, kCenter, kBottom };

class StaticText {
 public:
  StaticText(const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;

 private:

};

class DynamicText {
 public:
  DynamicText(const SharedBasicContext& context,
              const std::vector<std::string>& texts,
              Font font, int font_height);

  // This class is neither copyable nor movable.
  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  void Draw(const std::string& text,
            const glm::vec4& color_alpha,
            const glm::vec2& coord,
            AlignHorizontal align_horizontal,
            AlignVertical align_vertical) const;

 private:
  struct CharInfo {
    glm::ivec2 size;
    glm::ivec2 bearing;
    int offset_x;
  };

  absl::flat_hash_map<char, CharInfo> char_info_map;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H */
