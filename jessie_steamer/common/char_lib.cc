//
//  char_lib.cc
//
//  Created by Pujun Lun on 5/12/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/common/char_lib.h"

#include "absl/container/flat_hash_set.h"
#include "absl/memory/memory.h"
#include "jessie_steamer/common/util.h"

namespace jessie_steamer {
namespace common {

CharLib::CharLib(const std::vector<std::string>& texts,
                 const std::string& font_path, int font_height) {
  if (FT_Init_FreeType(&lib_)) {
    FATAL("Failed to init FreeType library");
  }

  if (FT_New_Face(lib_, font_path.c_str(), 0, &face_)) {
    FATAL("Failed to load font");
  }
  FT_Set_Pixel_Sizes(face_, 0, font_height);  // auto adjustment for width

  for (const auto& text : texts) {
    for (auto c : text) {
      if (char_info_map_.find(c) != char_info_map_.end()) {
        continue;
      }

      if (FT_Load_Char(face_, c, FT_LOAD_RENDER)) {
        FATAL("Failed to load glyph");
      }

      char_info_map_.emplace(c, CharInfo{
          /*bearing=*/{
              face_->glyph->bitmap_left,
              face_->glyph->bitmap_top,
          },
          // measured with number of 1/64 pixels
          /*advance=*/static_cast<unsigned int>(face_->glyph->advance.x) >> 6,
          /*image=*/absl::make_unique<Image>(
              /*width=*/face_->glyph->bitmap.width,
              /*height=*/face_->glyph->bitmap.rows,
              /*channel=*/1,
              /*raw_data=*/face_->glyph->bitmap.buffer,
              /*flip_y=*/true
          ),
      });
    }
  }
}

CharLib::~CharLib() {
  FT_Done_Face(face_);
  FT_Done_FreeType(lib_);
}

} /* namespace common */
} /* namespace jessie_steamer */
