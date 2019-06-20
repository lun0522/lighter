//
//  text.cc
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#include "jessie_steamer/wrapper/vulkan/text.h"

#include "absl/memory/memory.h"
#include "jessie_steamer/common/char_lib.h"
#include "jessie_steamer/common/util.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {
namespace {

using std::string;

// alignment requirement:
// https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/chap14.html#interfaces-resources-layout
struct DrawCharInfo {
  alignas(16) glm::vec4 x_coords;
  alignas(16) glm::vec4 y_coords;
  alignas(16) glm::vec4 color_alpha;
};

string GetFontPath(Font font) {
  const string prefix = "external/resource/font/";
  switch (font) {
    case Font::kGeorgia:
      return prefix + "georgia.ttf";
    case Font::kOstrich:
      return prefix + "ostrich.ttf";
  }
}

} /* namespace */

StaticText::StaticText(const std::vector<string>& texts,
                       Font font, int font_height) {
  common::CharLib lib{texts, GetFontPath(font), font_height};
}

DynamicText::DynamicText(const SharedBasicContext& context,
                         const std::vector<string>& texts,
                         Font font, int font_height) {
  const common::CharLib lib{texts, GetFontPath(font), font_height};
  if (lib.char_info_map().empty()) {
    FATAL("No character loaded");
  }

  absl::flat_hash_map<char, std::unique_ptr<TextureImage>> char_tex_map;
  int offset_x = 0;
  for (const auto& ch : lib.char_info_map()) {
    const auto& info = ch.second;
    char_info_map[ch.first] = CharInfo{
        info.size,
        info.bearing,
        offset_x,
    };
    char_tex_map[ch.first] = absl::make_unique<TextureImage>(
        context, TextureBuffer::Info{
            {info.data},
            VK_FORMAT_R8_UNORM,
            static_cast<uint32_t>(info.size.x),
            static_cast<uint32_t>(info.size.y),
            /*channel=*/1,
        }),
    offset_x += info.advance;
  }

  const int total_width = offset_x;
  OffscreenImage image{context, /*channel=*/1,
                       {static_cast<uint32_t>(total_width),
                        static_cast<uint32_t>(font_height)}};

  PipelineBuilder pipeline_builder{context};
  pipeline_builder
      .add_shader({VK_SHADER_STAGE_VERTEX_BIT,
                   "jessie_steamer/shader/vulkan/simple_2d.vert.spv"})
      .add_shader({VK_SHADER_STAGE_FRAGMENT_BIT,
                   "jessie_steamer/shader/vulkan/simple_2d.frag.spv"})
      .disable_depth_test();

  auto pipeline = pipeline_builder.Build();
}

void DynamicText::Draw(const string& text,
                       const glm::vec4& color_alpha,
                       const glm::vec2& coord,
                       AlignHorizontal align_horizontal,
                       AlignVertical align_vertical) const {

}

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */
