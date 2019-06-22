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

  CharLoader(SharedBasicContext context,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  CharLoader(const CharLoader&) = delete;
  CharLoader& operator=(const CharLoader&) = delete;

 private:
  struct CharResourceInfo {
    glm::ivec2 size;
    glm::ivec2 bearing;
    int offset_x;
    std::unique_ptr<TextureImage> image;
  };

  absl::flat_hash_map<char, CharResourceInfo> LoadCharResource(
      const common::CharLib& char_lib, int* total_width) const;

  std::unique_ptr<RenderPass> CreateRenderPass(
      const VkExtent2D& target_extent,
      RenderPassBuilder::GetImage&& get_target_image) const;

  std::vector<StaticDescriptor> CreateDescriptors() const;

  std::unique_ptr<Pipeline> CreatePipeline(
      const VkExtent2D& target_extent, const RenderPass& render_pass,
      const std::vector<StaticDescriptor>& descriptors) const;

  SharedBasicContext context_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif //JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_UTIL_H
