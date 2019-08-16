//
//  char_lib.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_CHAR_LIB_H
#define JESSIE_STEAMER_COMMON_CHAR_LIB_H

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "jessie_steamer/common/file.h"
#include "third_party/freetype/ft2build.h"
#include FT_FREETYPE_H
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

// Character library backed by FreeType.
class CharLib {
 public:
  // Information related to drawing the character. For details, see:
  // https://learnopengl.com/img/in-practice/glyph.png
  struct CharInfo {
    glm::ivec2 bearing;
    unsigned int advance_x;
    std::unique_ptr<Image> image;
  };

  // We will load all characters in 'texts' from the library. All of them will
  // be of height 'font_height', while the width is self-adjusted.
  CharLib(const std::vector<std::string>& texts,
          const std::string& font_path, int font_height);

  // This class is neither copyable nor movable.
  CharLib(const CharLib&) = delete;
  CharLib& operator=(const CharLib&) = delete;

  ~CharLib() = default;

  // Accessors.
  const absl::flat_hash_map<char, CharInfo>& char_info_map() const {
    return char_info_map_;
  }

 private:
  // Holds information about loaded characters. Only those characters loaded
  // via the constructor will be in this map.
  absl::flat_hash_map<char, CharInfo> char_info_map_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CHAR_LIB_H */
