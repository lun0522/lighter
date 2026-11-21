//
//  text_util.h
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef LIGHTER_RENDERER_EXTENSION_TEXT_UTIL_H
#define LIGHTER_RENDERER_EXTENSION_TEXT_UTIL_H

#include <array>
#include <cstdint>
#include <format>
#include <glm/glm.hpp>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "lighter/common/char_lib.h"
#include "lighter/common/util.h"
#include "lighter/renderer/ir/image.h"
#include "lighter/renderer/ir/renderer.h"

namespace lighter::renderer::extension {

// Renders all characters used in 'texts' onto one font atlas image so strings
// made of any combination of those characters can be drawn using one texture.
class CharLoader {
 public:
  enum class Font { kGeorgia, kOstrich };

  // Glyph metrics on the character atlas image (all coordinates in [0.0, 1.0]).
  struct CharTextureInfo {
    glm::vec2 size;
    glm::vec2 bearing;
    float offset_x;
    float advance_x;
  };

  using CharTextureInfoMap = std::unordered_map<char, CharTextureInfo>;

  CharLoader(ir::Renderer& renderer, std::span<const std::string> texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  CharLoader(const CharLoader&) = delete;
  CharLoader& operator=(const CharLoader&) = delete;

  float GetAspectRatio() const {
    return static_cast<float>(char_atlas_image_->width()) /
           static_cast<float>(char_atlas_image_->height());
  }

  const ir::Image& atlas_image() const { return *char_atlas_image_; }
  float space_advance() const {
    ASSERT_HAS_VALUE(space_advance_x_, "Space is not loaded");
    return space_advance_x_.value();
  }
  const CharTextureInfoMap& char_texture_info_map() const {
    return char_texture_info_map_;
  }
  const CharTextureInfo& char_texture_info(char character) const {
    const auto iter = char_texture_info_map_.find(character);
    ASSERT_FALSE(iter == char_texture_info_map_.end(),
                 std::format("'{}' was not loaded", character));
    return iter->second;
  }

 private:
  std::unique_ptr<ir::Image> char_atlas_image_;
  std::optional<float> space_advance_x_;
  CharTextureInfoMap char_texture_info_map_;
};

// Pre-bakes each string in 'texts' onto its own texture via CharLoader so
// static labels can be rendered with a single textured quad.
class TextLoader {
 public:
  struct TextTextureInfo {
    float aspect_ratio;
    float base_y;
    std::unique_ptr<ir::Image> image;
  };

  TextLoader(ir::Renderer& renderer, std::span<const std::string> texts,
             CharLoader::Font font, int font_height);

  // This class is neither copyable nor movable.
  TextLoader(const TextLoader&) = delete;
  TextLoader& operator=(const TextLoader&) = delete;

  const TextTextureInfo& texture_info(int text_index) const {
    return text_texture_infos_.at(text_index);
  }

 private:
  TextTextureInfo CreateTextTexture(ir::Renderer& renderer,
                                    const std::string& text, int font_height,
                                    const CharLoader& char_loader) const;

  std::vector<TextTextureInfo> text_texture_infos_;
};

namespace text {

constexpr int kNumVerticesPerRect = 4;
constexpr int kNumIndicesPerRect = 6;

// Returns the canonical single-quad index pattern {0, 1, 2, 0, 2, 3}.
const std::array<uint32_t, kNumIndicesPerRect>& GetIndicesPerRect();

// Generates sequential quad index triples for 'num_rects' rectangles.
std::vector<uint32_t> GenerateQuadIndices(int num_rects);

// Returns the default sampler descriptor used when sampling text textures.
ir::SamplerDescriptor GetTextSamplerDescriptor();

// Appends quad vertices for a rectangle in [0.0, 1.0] normalized coordinates
// (converted internally to [-1.0, 1.0] NDC).
void AppendCharPosAndTexCoord(const glm::vec2& pos_bottom_left,
                              const glm::vec2& pos_increment,
                              const glm::vec2& tex_coord_bottom_left,
                              const glm::vec2& tex_coord_increment,
                              std::vector<common::Vertex2D>* vertices);

// Appends quad vertices for all characters in 'text' and returns the final X
// advance coordinate in [0.0, 1.0].
float LoadCharsVertexData(const std::string& text,
                          const CharLoader& char_loader, const glm::vec2& ratio,
                          float initial_offset_x, float base_y,
                          std::vector<common::Vertex2D>* vertices);

}  // namespace text
}  // namespace lighter::renderer::extension

#endif  // LIGHTER_RENDERER_EXTENSION_TEXT_UTIL_H
