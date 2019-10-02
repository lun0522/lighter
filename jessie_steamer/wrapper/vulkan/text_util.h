//
//  text_util.h
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/common/char_lib.h"
#include "jessie_steamer/common/file.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class CharLoader {
 public:
  enum class Font { kGeorgia, kOstrich };

  // All numbers are in range [0.0, 1.0].
  struct CharTextureInfo {
    glm::vec2 size;
    glm::vec2 bearing;
    float offset_x;
    float advance_x;
  };

  CharLoader(SharedBasicContext context,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  CharLoader(const CharLoader&) = delete;
  CharLoader& operator=(const CharLoader&) = delete;

  OffscreenImagePtr texture() const { return image_.get(); }
  float width_height_ratio() const { return width_height_ratio_; }
  absl::optional<float> space_advance() const { return space_advance_x_; }

  const absl::flat_hash_map<char, CharTextureInfo>& char_texture_map() const {
    return char_texture_map_;
  };

 private:
  struct CharTextures {
    absl::flat_hash_map<char, std::unique_ptr<TextureImage>> char_image_map;
    VkExtent2D extent_after_merge;
  };

  // Populates 'char_texture_map_' and 'space_advance_x_'.
  CharTextures CreateCharTextures(const common::CharLib& char_lib,
                                  int font_height);

  const SharedBasicContext context_;
  std::unique_ptr<OffscreenImage> image_;
  float width_height_ratio_;
  absl::optional<float> space_advance_x_;
  absl::flat_hash_map<char, CharTextureInfo> char_texture_map_;
};

class TextLoader {
 public:
  struct TextTexture {
    float width_height_ratio;
    float base_y;
    std::unique_ptr<OffscreenImage> image;
  };

  TextLoader(SharedBasicContext context,
             const std::vector<std::string>& texts,
             CharLoader::Font font, int font_height);

  // This class is neither copyable nor movable.
  TextLoader(const TextLoader&) = delete;
  TextLoader& operator=(const TextLoader&) = delete;

  const TextTexture& texture(int index) const {
    return text_textures_.at(index);
  }

 private:
  TextTexture CreateTextTexture(const std::string& text, int font_height,
                                const CharLoader& char_loader,
                                StaticDescriptor* descriptor,
                                RenderPassBuilder* render_pass_builder,
                                PipelineBuilder* pipeline_builder,
                                DynamicPerVertexBuffer* vertex_buffer) const;

  const SharedBasicContext context_;
  std::vector<TextTexture> text_textures_;
};

namespace text_util {

constexpr int kNumVerticesPerRect = 4;
constexpr int kNumIndicesPerRect = 6;

// Returns indices per rectangle.
const std::array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect();

// Returns the data size used for vertex buffer. Is is assumed that indices will
// be shared and each vertex data is of type VertexAttrib2D.
inline int GetVertexDataSize(int num_rects) {
  return sizeof(GetIndicesPerRect()[0]) * kNumIndicesPerRect +
         sizeof(common::VertexAttrib2D) * kNumVerticesPerRect * num_rects;
}

// Appends pos and tex_coord to 'vertices' in CCW order.
// All numbers should be in range [0.0, 1.0]. Pos will be normalized internally.
void AppendCharPosAndTexCoord(const glm::vec2& pos_bottom_left,
                              const glm::vec2& pos_increment,
                              const glm::vec2& tex_coord_bottom_left,
                              const glm::vec2& tex_coord_increment,
                              std::vector<common::VertexAttrib2D>* vertices);

// Fills 'vertex_buffer' with data of characters in 'text', and returns the
// right boundary of rendered text (i.e. final x offset).
float LoadCharsVertexData(const std::string& text,
                          const CharLoader& char_loader,
                          const glm::vec2& ratio, float initial_offset_x,
                          float base_y, bool flip_y,
                          DynamicPerVertexBuffer* vertex_buffer);

} /* namespace text_util */
} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H
