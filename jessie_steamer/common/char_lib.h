//
//  char_lib.h
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_COMMON_CHAR_LIB_H
#define JESSIE_STEAMER_COMMON_CHAR_LIB_H

#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "third_party/freetype/ft2build.h"
#include FT_FREETYPE_H
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace common {

class CharLib {
 public:
  struct Character {
    glm::ivec2 size;
    glm::ivec2 bearing;
    unsigned int advance;
    unsigned char* data;
  };

  CharLib(const std::vector<std::string>& texts,
          const std::string& font_path,
          glm::uvec2 font_size);

  // This class is neither copyable nor movable.
  CharLib(const CharLib&) = delete;
  CharLib& operator=(const CharLib&) = delete;

  ~CharLib();

  const Character& operator[](char c) { return chars_[c]; }

 private:
  absl::flat_hash_map<char, Character> chars_;
  FT_Library lib_;
  FT_Face face_;
};

} /* namespace common */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_COMMON_CHAR_LIB_H */
