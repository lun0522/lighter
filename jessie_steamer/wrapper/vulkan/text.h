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
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

enum class Font {
  kGeorgia,
  kOstrich,
};

class StaticText {
 public:
  StaticText(const std::vector<std::string>& texts,
             Font font, glm::uvec2 font_size);

 private:

};

class DynamicText {
 public:
  DynamicText(const std::vector<std::string>& texts,
              Font font, glm::uvec2 font_size);

 private:
  struct CharInfo {
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
    TextureImage image;
  };

  absl::flat_hash_map<char, std::unique_ptr<CharInfo>> char_info_map;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
