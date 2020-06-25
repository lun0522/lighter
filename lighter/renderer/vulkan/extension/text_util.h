//
//  text_util.h
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_VULKAN_TEXT_UTIL_H
#define LIGHTER_RENDERER_VULKAN_TEXT_UTIL_H

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "lighter/common/char_lib.h"
#include "lighter/common/file.h"
#include "lighter/common/util.h"
#include "lighter/renderer/vulkan/wrapper/buffer.h"
#include "lighter/renderer/vulkan/wrapper/basic_context.h"
#include "lighter/renderer/vulkan/wrapper/descriptor.h"
#include "lighter/renderer/vulkan/wrapper/image.h"
#include "lighter/renderer/vulkan/wrapper/pipeline.h"
#include "lighter/renderer/vulkan/wrapper/render_pass_util.h"
#include "third_party/absl/container/flat_hash_map.h"
#include "third_party/absl/types/optional.h"
#include "third_party/absl/types/span.h"
#include "third_party/glm/glm.hpp"

namespace lighter {
namespace renderer {
namespace vulkan {

// This class is used to render all characters that might be used later onto
// a font atlas image, so that we can render those characters in any combination
// with only one render call, binding only one texture. The user can query the
// glyph information of each character from char_texture_info_map(). Note that
// we don't render the space character onto the character atlas image. To query
// the advance of space, the user should include at least one space in any of
// 'texts', and call space_advance().
// For now we only support the horizontal layout.
class CharLoader {
 public:
  // Fonts that are supported.
  enum class Font { kGeorgia, kOstrich };

  // Contains the information about the glyph of a character on the
  // 'char_atlas_image_'. All numbers are in range [0.0, 1.0].
  struct CharTextureInfo {
    glm::vec2 size;
    glm::vec2 bearing;
    float offset_x;
    float advance_x;
  };

  // Maps each character to its texture information.
  using CharTextureInfoMap = absl::flat_hash_map<char, CharTextureInfo>;

  // 'texts' must contain all characters that might be rendered using this
  // loader. Note that this does not mean the user can only use this to render
  // elements of 'texts'. The user may use any combination of these characters.
  CharLoader(const SharedBasicContext& context,
             absl::Span<const std::string> texts, Font font, int font_height);

  // This class is neither copyable nor movable.
  CharLoader(const CharLoader&) = delete;
  CharLoader& operator=(const CharLoader&) = delete;

  // Returns the aspect ratio of the character atlas image.
  float GetAspectRatio() const {
    return util::GetAspectRatio(char_atlas_image_->extent());
  }

  // Accessors.
  OffscreenImagePtr atlas_image() const { return char_atlas_image_.get(); }
  float space_advance() const {
    ASSERT_HAS_VALUE(space_advance_x_, "Space is not loaded");
    return space_advance_x_.value();
  }
  const CharTextureInfoMap& char_texture_info_map() const {
    return char_texture_info_map_;
  }
  const CharTextureInfo& char_texture_info(char character) const {
    const auto found = char_texture_info_map_.find(character);
    ASSERT_FALSE(found == char_texture_info_map_.end(),
                 absl::StrFormat("'%c' was not loaded", character));
    return found->second;
  }

 private:
  // Maps each character to its texture image.
  using CharImageMap = absl::flat_hash_map<char, std::unique_ptr<TextureImage>>;

  // Computes the extent of 'char_atlas_image_'. The width will be the total
  // width of characters (excluding space) in 'char_lib', and the height will be
  // the same to that of the tallest character.
  VkExtent2D GetCharAtlasImageExtent(const common::CharLib& char_lib,
                                     int interval_between_chars) const;

  // Returns the horizontal advance of space character. If space is not loaded
  // in 'char_lib', returns absl::nullopt.
  absl::optional<float> GetSpaceAdvanceX(const common::CharLib& char_lib,
                                         const Image& target_image) const;

  // Populates 'char_texture_map' and 'char_texture_info_map' with characters
  // loaded in 'char_lib', excluding the space character.
  void CreateCharTextures(
      const SharedBasicContext& context,
      const common::CharLib& char_lib,
      int interval_between_chars, const Image& target_image,
      CharImageMap* char_texture_map,
      CharTextureInfoMap* char_texture_info_map) const;

  // Creates a vertex buffer for rendering characters in 'char_merge_order',
  // which should not include the space character.
  std::unique_ptr<StaticPerVertexBuffer> CreateVertexBuffer(
      const SharedBasicContext& context,
      const std::vector<char>& char_merge_order) const;

  // Character atlas image.
  std::unique_ptr<OffscreenImage> char_atlas_image_;

  // We don't need to render the space character. Instead, we only record
  // its advance.
  absl::optional<float> space_advance_x_;

  // Maps each character to its glyph information on 'char_atlas_image_'.
  CharTextureInfoMap char_texture_info_map_;
};

// This class is used to render each element of 'texts' onto one texture, so
// that later we only need to bind one texture to render any element of 'texts'.
// For now we only support the horizontal layout.
class TextLoader {
 public:
  // Contains information required for rendering a text.
  struct TextTextureInfo {
    float aspect_ratio;
    float base_y;
    std::unique_ptr<OffscreenImage> image;
  };

  // The loader will be able to render any of 'texts'.
  TextLoader(const SharedBasicContext& context,
             absl::Span<const std::string> texts,
             CharLoader::Font font, int font_height);

  // This class is neither copyable nor movable.
  TextLoader(const TextLoader&) = delete;
  TextLoader& operator=(const TextLoader&) = delete;

  // Accessors.
  const TextTextureInfo& texture_info(int text_index) const {
    return text_texture_infos_.at(text_index);
  }

 private:
  // Creates texture for 'text'.
  TextTextureInfo CreateTextTexture(
      const SharedBasicContext& context,
      const std::string& text, int font_height,
      const CharLoader& char_loader,
      StaticDescriptor* descriptor,
      NaiveRenderPassBuilder* render_pass_builder,
      GraphicsPipelineBuilder* pipeline_builder,
      DynamicPerVertexBuffer* vertex_buffer) const;

  // Texture information of each element of 'texts' passed to the constructor.
  std::vector<TextTextureInfo> text_texture_infos_;
};

namespace text {

constexpr int kNumVerticesPerRect = 4;
constexpr int kNumIndicesPerRect = 6;

// Returns indices used for drawing a rectangle.
const std::array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect();

// Returns the data size used for vertex buffer. Is is assumed that indices will
// be shared and each vertex data is of type Vertex2D.
inline size_t GetVertexDataSize(int num_rects) {
  return sizeof(GetIndicesPerRect()[0]) * kNumIndicesPerRect +
         sizeof(common::Vertex2D) * kNumVerticesPerRect * num_rects;
}

// Appends pos and tex_coord to 'vertices'.
// All numbers should be in range [0.0, 1.0]. Pos will be normalized internally.
void AppendCharPosAndTexCoord(const glm::vec2& pos_bottom_left,
                              const glm::vec2& pos_increment,
                              const glm::vec2& tex_coord_bottom_left,
                              const glm::vec2& tex_coord_increment,
                              std::vector<common::Vertex2D>* vertices);

// Appends the vertex data of characters in 'text' to the end of 'vertices',
// and returns the right boundary of rendered text (i.e. final X offset).
float LoadCharsVertexData(const std::string& text,
                          const CharLoader& char_loader, const glm::vec2& ratio,
                          float initial_offset_x, float base_y,
                          std::vector<common::Vertex2D>* vertices);

} /* namespace text */
} /* namespace vulkan */
} /* namespace renderer */
} /* namespace lighter */

#endif /* LIGHTER_RENDERER_VULKAN_TEXT_UTIL_H */
