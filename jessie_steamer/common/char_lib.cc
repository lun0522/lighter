//
//  char_lib.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/char_lib.h"

#include "absl/container/flat_hash_set.h"
#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace common {

CharLib::CharLib(const std::vector<std::string>& texts,
                 const std::string& font_path,
                 glm::uvec2 font_size) {
  if (FT_Init_FreeType(&lib_)) {
    FATAL("Failed to init FreeType library");
  }

  if (FT_New_Face(lib_, font_path.c_str(), 0, &face_)) {
    FATAL("Failed to load font");
  }
  FT_Set_Pixel_Sizes(face_, font_size.x, font_size.y);

  absl::flat_hash_set<char> to_load{};
  for (const auto& text : texts) {
    for (auto c : text) {
      to_load.emplace(c);
    }
  }

  for (auto c : to_load) {
    if (FT_Load_Char(face_, c, FT_LOAD_RENDER)) {
      FATAL("Failed to load glyph");
    }

    Character ch{
        /*size=*/{face_->glyph->bitmap.width,
                  face_->glyph->bitmap.rows},
        /*bearing=*/{face_->glyph->bitmap_left,
                     face_->glyph->bitmap_top},
        // measured with number of 1/64 pixels
        /*advance=*/static_cast<unsigned int>(face_->glyph->advance.x) >> 6,
        /*data=*/face_->glyph->bitmap.buffer,
    };
    chars_.emplace(c, ch);
  }
}

CharLib::~CharLib() {
  FT_Done_Face(face_);
  FT_Done_FreeType(lib_);
}

} /* namespace common */
} /* namespace jessie_steamer */
