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

class CharLib {
 public:
  // https://learnopengl.com/img/in-practice/glyph.png
  struct CharInfo {
    glm::ivec2 bearing;
    unsigned int advance;
    std::unique_ptr<Image> image;
  };

  CharLib(const std::vector<std::string>& texts,
          const std::string& font_path, int font_height);

  // This class is neither copyable nor movable.
  CharLib(const CharLib&) = delete;
  CharLib& operator=(const CharLib&) = delete;

  ~CharLib();

  const absl::flat_hash_map<char, CharInfo>& char_info_map() const {
    return char_info_map_;
  }

 private:
  absl::flat_hash_map<char, CharInfo> char_info_map_;
  FT_Library lib_;
  FT_Face face_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CHAR_LIB_H */
