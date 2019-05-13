//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include "jessie_steamer/common/char_lib.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;

string GetFontPath(Font font) {
  const string prefix = "jessie_steamer/resource/font/";
  switch (font) {
    case Font::kGeorgia:
      return prefix + "georgia.ttf";
    case Font::kOstrich:
      return prefix + "ostrich.ttf";
  }
};

} /* namespace */

StaticText::StaticText(const std::vector<string>& texts,
                       Font font, glm::uvec2 font_size) {
  common::CharLib lib{texts, GetFontPath(font), font_size};
}

DynamicText::DynamicText(const std::vector<std::string>& texts,
                         Font font, glm::uvec2 font_size) {
  common::CharLib lib{texts, GetFontPath(font), font_size};
}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
