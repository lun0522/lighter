//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <string>
#include <vector>

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

};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
