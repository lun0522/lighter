//
//  text.h
//
//  Created by Pujun Lun on 4/7/19.
//  Copyright © 2019 Pujun Lun. All rights reserved.
//

#ifndef JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H
#define JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H

#include <functional>
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

  // Should be called after initialization and whenever frame is resized.
  void Update(const VkExtent2D& frame_size,
              const RenderPass& render_pass, uint32_t subpass_index);

 protected:
  Text(SharedBasicContext context, int num_frames);
  void UpdateUniformBuffer(int frame, const glm::vec3& color, float alpha);

  const SharedBasicContext context_;
  DynamicPerVertexBuffer vertex_buffer_;
  std::unique_ptr<UniformBuffer> uniform_buffer_;
  PipelineBuilder pipeline_builder_;
  std::unique_ptr<Pipeline> pipeline_;
};

class StaticText : public Text {
 public:
  StaticText(SharedBasicContext context,
             int num_frames,
             const std::vector<std::string>& texts,
             Font font, int font_height);

  // This class is neither copyable nor movable.
  StaticText(const StaticText&) = delete;
  StaticText& operator=(const StaticText&) = delete;

  // Renders text and returns left and right boundary.
  glm::vec2 Draw(const VkCommandBuffer& command_buffer,
                 int frame, const VkExtent2D& frame_size, int text_index,
                 const glm::vec3& color, float alpha, float height,
                 float base_x, float base_y, Align align);

 private:
  TextLoader text_loader_;
  std::vector<std::unique_ptr<DynamicDescriptor>> descriptors_;
  std::vector<std::function<void(const VkCommandBuffer& command_buffer,
                                 const VkPipelineLayout& pipeline_layout,
                                 int text_index)>> push_descriptors_;
};

class DynamicText : public Text {
 public:
  DynamicText(SharedBasicContext context,
              int num_frames,
              const std::vector<std::string>& texts,
              Font font, int font_height);

  // This class is neither copyable nor movable.
  DynamicText(const DynamicText&) = delete;
  DynamicText& operator=(const DynamicText&) = delete;

  // Renders text and returns left and right boundary.
  glm::vec2 Draw(const VkCommandBuffer& command_buffer,
                 int frame, const VkExtent2D& frame_size,
                 const std::string& text, const glm::vec3& color, float alpha,
                 float height, float base_x, float base_y, Align align);

 private:
  CharLoader char_loader_;
  std::vector<std::unique_ptr<StaticDescriptor>> descriptors_;
};

} /* namespace vulkan */
} /* namespace wrapper */
} /* namespace jessie_steamer */

#endif /* JESSIE_STEAMER_WRAPPER_VULKAN_TEXT_H */
