//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdexcept>
#include <unordered_map>

#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::runtime_error;

std::unordered_map<char, Character> LoadCharLib(const std::string& font_path,
                                                glm::uvec2 font_size) {
  std::unordered_map<char, Character> loaded_chars;

  FT_Library lib;
  if (FT_Init_FreeType(&lib)) {
    throw runtime_error{"Failed to init FreeType library"};
  }

  FT_Face face;
  if (FT_New_Face(lib, font_path.c_str(), 0, &face)) {
    throw runtime_error{"Failed to load font"};
  }

  FT_Set_Pixel_Sizes(face, font_size.x, font_size.y);

  for (unsigned int c = 0; c < 128; ++c) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      throw runtime_error{"Failed to load glyph"};
    }

    Character ch{
        glm::vec2{face->glyph->bitmap.width,
                  face->glyph->bitmap.rows},
        glm::vec2{face->glyph->bitmap_left,
                  face->glyph->bitmap_top},
        // advance is number of 1/64 pixels
        (unsigned int)face->glyph->advance.x >> 6,
    };
    loaded_chars.emplace(c, ch);
  }

  FT_Done_Face(face);
  FT_Done_FreeType(lib);

  return loaded_chars;
}

} /* namespace */

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
