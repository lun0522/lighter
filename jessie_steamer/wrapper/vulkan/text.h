//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <memory>
#include <string>
#include <vector>

#include "jessie_steamer/wrapper/vulkan/basic_context.h"
#include "jessie_steamer/wrapper/vulkan/buffer.h"
#include "jessie_steamer/wrapper/vulkan/descriptor.h"
#include "jessie_steamer/wrapper/vulkan/pipeline.h"
#include "jessie_steamer/wrapper/vulkan/render_pass.h"
#include "jessie_steamer/wrapper/vulkan/text_util.h"
#include "third_party/glm/glm.hpp"

namespace jessie_steamer {
namespace wrapper {
namespace vulkan {

class Text {
 public:
  using Font = CharLoader::Font;
  enum class Align { kLeft, kCenter, kRight };

  Text(SharedBasicContext context) : context_{std::move(context)} {}

 protected:
  SharedBasicContext context_;
};

class StaticText : public Text {
 public:
  StaticText(SharedBasicContext context,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;
};

class DynamicText : public Text {
 public:
  DynamicText(SharedBasicContext context,
              int num_frame,
              const std::vector<std::string>& texts,
              Font font, int font_height);

  // This class is neither copyable nor movable.
  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  // Should be called after initialization and whenever frame is resized.
  void Update(VkExtent2D frame_size,
              const RenderPass& render_pass, uint32_t subpass_index);

  void Draw(const VkCommandBuffer& command_buffer,
            int frame, VkExtent2D frame_size, const std::string& text,
            const glm::vec3& color, float alpha, float height,
            float horizontal_base, float vertical_base, Align align);

 private:
  CharLoader char_loader_;
  DynamicPerVertexBuffer vertex_buffer_;
  UniformBuffer uniform_buffer_;
  std::vector<std::unique_ptr<StaticDescriptor>> descriptors_;
  PipelineBuilder pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H */
