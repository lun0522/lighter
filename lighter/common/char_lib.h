//
//  char_lib.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_COMMON_CHAR_LIB_H
#define LIGHTER_COMMON_CHAR_LIB_H

#include <ft2build.h>

#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include FT_FREETYPE_H
#include <glm/glm.hpp>

#include "lighter/common/file.h"
#include "lighter/common/image.h"

namespace lighter::common {

// Character library backed by FreeType.
class CharLib {
 public:
  // Information related to drawing the character. For details, see:
  // https://learnopengl.com/img/in-practice/glyph.png
  struct CharInfo {
    glm::ivec2 bearing;
    glm::ivec2 advance;
    Image image;
  };

  // We will load all characters in 'texts' from the library. All of them will
  // be of height 'font_height', while the width is self-adjusted.
  CharLib(std::span<const std::string> texts, const std::string& font_path,
          int font_height, bool flip_y);

  // This class is neither copyable nor movable.
  CharLib(const CharLib&) = delete;
  CharLib& operator=(const CharLib&) = delete;

  // Accessors.
  const std::unordered_map<char, CharInfo>& char_info_map() const {
    return char_info_map_;
  }

 private:
  // Holds information about loaded characters. Only those characters loaded
  // via the constructor will be in this map.
  std::unordered_map<char, CharInfo> char_info_map_;
};

}  // namespace lighter::common

#endif  // LIGHTER_COMMON_CHAR_LIB_H
