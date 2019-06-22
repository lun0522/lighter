//
//  text_util.h
//
//  Created by Pujun Lun on 6/20/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "jessie_steamer/common/char_lib.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/image.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class CharLoader {
 public:
  enum class Font { kGeorgia, kOstrich };

  struct CharTextureInfo {
    glm::vec2 size;
    glm::vec2 bearing;
    float offset_x;
  };

  CharLoader(SharedBasicContext context,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  CharLoader(const CharLoader&) = delete;
  CharLoader& operator=(const CharLoader&) = delete;

  const absl::flat_hash_map<char, CharTextureInfo>& char_texture_map() const {
    return char_texture_map_;
  };

 private:
  struct CharTextures {
    absl::flat_hash_map<char, std::unique_ptr<TextureImage>> char_image_map;
    VkExtent2D extent_after_merge;
  };

  absl::flat_hash_map<char, CharLoader::CharTextureInfo> CreateCharTextureMap(
      const common::CharLib& char_lib, int font_height,
      CharTextures* char_textures) const;

  std::unique_ptr<RenderPass> CreateRenderPass(
      const VkExtent2D& target_extent,
      RenderPassBuilder::GetImage&& get_target_image) const;

  std::unique_ptr<DynamicDescriptor> CreateDescriptor() const;

  std::unique_ptr<Pipeline> CreatePipeline(
      const VkExtent2D& target_extent, const RenderPass& render_pass,
      const DynamicDescriptor& descriptor) const;

  std::unique_ptr<PerVertexBuffer> CreateVertexBuffer(
      std::vector<char>* char_merge_order) const;

  SharedBasicContext context_;
  std::unique_ptr<OffscreenImage> image_;
  absl::flat_hash_map<char, CharTextureInfo> char_texture_map_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H
