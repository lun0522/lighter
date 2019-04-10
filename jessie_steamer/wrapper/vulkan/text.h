//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <string>

#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

struct Character {
  glm::ivec2 size;
  glm::ivec2 bearing;
  unsigned int advance;
};


} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
